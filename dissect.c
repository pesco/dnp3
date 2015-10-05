#include <dnp3.h>
#include <hammer/hammer.h>
#include <hammer/glue.h>
#include "src/hammer.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>

#include "dissect.h"


#define BUFLEN 4619 // enough for 4096B over 1 frame or 355 empty segments
#define CTXMAX 1024 // maximum number of connection contexts
#define TBUFLEN (BUFLEN/13*2)   // 13 = min. size of a frame
                                // 2  = max. number of tokens per frame

static uint8_t buf[BUFLEN];     // global input buffer

struct Context {
    struct Context *next;

    uint16_t src;
    uint16_t dst;

    // transport function
    const DNP3_Segment *last_segment;
    HSuspendedParser *tfun;
    size_t tfun_pos;        // number of bytes consumed so far

    // raw valid frames
    uint8_t buf[BUFLEN];
    size_t n;
};

static struct Context *contexts = NULL;    // linked list

static LogCallback cb_log;
static OutputCallback cb_out;
static void *cb_env;


static void error(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    cb_log(cb_env, LOG_ERR, fmt, args);
    va_end(args);
}

static void debug(const char *fmt, ...)
{
#if 0
    va_list args;

    va_start(args, fmt);
    cb_log(cb_env, LOG_DEBUG, fmt, args);
    va_end(args);
#endif
}

static void print(const char *fmt, ...)
{
    va_list args;
    char buf[1024];
    int n;

    va_start(args, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if(n >= 0)
        cb_out(cb_env, (uint8_t *)buf, n);
    else
        error("vsnprintf: %s", strerror(errno));
}


HParser *dnp3_p_synced_frame;       // skips bytes until valid frame header
HParser *dnp3_p_transport_function; // the transport-layer state machine
HParser *dnp3_p_app_message;        // application request or response


static bool segment_equal(const DNP3_Segment *a, const DNP3_Segment *b)
{
    // a and b must be byte-by-byte identical
    return (a->fir == b->fir &&
            a->fin == b->fin &&
            a->seq == b->seq &&
            a->len == b->len &&
            (a->payload == b->payload ||    // catches NULL case
             memcmp(a->payload, b->payload, a->len) == 0));
}

// Define an alphabet of input events related to the transport function:
//
//  A   a segment arrived with the FIR bit set
//  =   a segment arrived with FIR unset and is bit-identical to the last
//  +   a segment arrived with FIR unset and seq == (lastseq+1)%64
//  !   a segment arrived with FIR unset and seq != (lastseq+1)%64
//  _   a segment arrived with FIR unset and there was no previous segment
//  Z   the last segment had the FIN bit set
//
// The transport function state machine is described by the regular expression
//
//      (A[+=]*Z|.)*
//
// with greedy matching.
//
// NB: Convert to a finite state machine and compare with IEEE 1815-2012
//     Figure 8-4 "Reception state diagram" (page 273)!
//
// We will use an unambiguous variant:
//
//      (A+[+=]*(Z|[^AZ+=])|[^A])*
//

// convert an incoming transport segment into appropriate input tokens
// precondition: p and t point to buffers of size >=2
// returns: number of tokens generated
static size_t transport_tokens(const DNP3_Segment *seg, const DNP3_Segment *last,
                               uint8_t *p, const DNP3_Segment **t)
{
    size_t n = 0;

    // first token
    if(seg->fir) {
        p[n] = 'A';
    } else if(last) {
        if(segment_equal(seg, last))
            p[n] = '=';
        else if(seg->seq == (last->seq + 1)%64)
            p[n] = '+';
        else
            p[n] = '!';
    } else {
        p[n] = '_';
    }
    t[n] = seg;
    n++;

    // second token
    t[n] = NULL;
    if(seg->fin)
        p[n++] = 'Z';

    return n;
}


static const DNP3_Segment **ttok_values;
static size_t ttok_pos = 0;
static HParsedToken *act_ttok(const HParseResult *p, void *user)
{
    DNP3_Segment **values = (DNP3_Segment **)ttok_values;   // XXX drop const
    assert(ttok_values != NULL);

    if(!p->ast)
        return NULL;

    debug("tok index %zu, ttok_pos=%zu\n", p->ast->index, ttok_pos);
    assert(p->ast->index >= ttok_pos);
    return H_MAKE(DNP3_Segment, values[p->ast->index - ttok_pos]);
}
static HParser *ttok(const HParser *p)
{
    return h_action(p, act_ttok, NULL);
}

// XXX move back down
const char *errorname(DNP3_ParseError e)
{
    static char s[] = "???";

    switch(e) {
    case ERR_FUNC_NOT_SUPP: return "FUNC_NOT_SUPP";
    case ERR_OBJ_UNKNOWN:   return "OBJ_UNKNOWN";
    case ERR_PARAM_ERROR:   return "PARAM_ERROR";
    default:
        snprintf(s, sizeof(s), "%d", (int)e);
        return s;
    }
}

// re-assemble a transport-layer segment series
static HParsedToken *act_series(const HParseResult *p, void *user)
{
    // p = (segment, segment*, NULL)    <- valid series
    //   | (segment, segment*)          <- invalid
    //        A        [+]*     Z?

    DNP3_Segment  *x  = H_FIELD(DNP3_Segment, 0);
    HCountedArray *xs = H_FIELD_SEQ(1);

    // if last element not present, this was not a valid series -> discard!
    if(p->ast->seq->used < 3)
        return NULL;

    // calculate payload length
    size_t len = x->len;
    for(size_t i=0; i<xs->used; i++) {
        len += H_CAST(DNP3_Segment, xs->elements[i])->len;
    }

    // assemble segments
    uint8_t *t = h_arena_malloc(p->arena, len);
    uint8_t *s = t;
    memcpy(s, x->payload, x->len);
    s += x->len;
    for(size_t i=0; i<xs->used; i++) {
        x = H_CAST(DNP3_Segment, xs->elements[i]);
        memcpy(s, x->payload, x->len);
        s += x->len;
    }

    // XXX move this out as well?
    print("T: reassembled payload:");
    for(size_t i=0; i<len; i++)
        print(" %.2X", (unsigned int)t[i]);
    print("\n");

    // try to parse a message  XXX move out of here
    HParseResult *r = h_parse(dnp3_p_app_message, t, len);
    if(r) {
        assert(r->ast != NULL);
        if(H_ISERR(r->ast->token_type)) {
            print("A: error %s\n", errorname(r->ast->token_type));
        } else {
            DNP3_Fragment *fragment = H_CAST(DNP3_Fragment, r->ast);
            print("A> %s\n", dnp3_format_fragment(fragment));
        }
    } else {
        print("A: no parse\n");
    }

    return H_MAKE_BYTES(t, len);
}

static bool not_err(HParseResult *p, void *user)
{
    return !H_ISERR(p->ast->token_type);
}
#define validate_request not_err
#define validate_response not_err

void init(void)
{
    dnp3_p_init();

    HParser *sync = h_indirect();
    H_RULE(sync_, h_choice(dnp3_p_link_frame, h_right(h_uint8(), sync), NULL));
        // XXX is it correct to skip one byte looking for the frame start?
    h_bind_indirect(sync, sync_);

    // transport-layer input tokens
    H_RULE (A,      ttok(h_ch('A')));
    H_RULE (Z,      h_ch('Z'));
    H_RULE (pls,    ttok(h_ch('+')));
    H_RULE (equ,    h_ch('='));

    H_RULE (notAZpe, h_not_in("AZ+=",4));
    H_RULE (notA,    h_not_in("A",1));

    // transport function: (A+[+=]*(Z|[^AZ+=])|[^A])*
    H_RULE (pe,      h_many(h_choice(pls, h_ignore(equ), NULL)));
    H_RULE (end,     h_choice(Z, h_ignore(notAZpe), NULL));
    H_RULE (A1,         h_indirect());
    h_bind_indirect(A1, h_choice(h_right(A, A1), A, NULL));
    H_ARULE(series,  h_sequence(A1, pe, end, NULL));
    H_RULE (tfun,    h_many(h_choice(series, h_ignore(notA), NULL)));

    int tfun_compile = !h_compile(tfun, PB_LALR, NULL);
    assert(tfun_compile);

    H_VRULE(request,  dnp3_p_app_request);
    H_VRULE(response, dnp3_p_app_response);
    H_RULE (message,  h_choice(request, response, NULL));

    dnp3_p_synced_frame = sync;
    dnp3_p_transport_function = tfun;
    dnp3_p_app_message = message;
}


void reset_tfun(struct Context *ctx)
{
    debug("tfun reset\n");
    if(ctx->tfun) {
        HParseResult *r = h_parse_finish(ctx->tfun);
        assert(r != NULL);
        h_parse_result_free(r);
        ctx->tfun = NULL;
    }
}

void init_tfun(struct Context *ctx)
{
    debug("tfun init\n");
    assert(ctx->tfun == NULL);
    ctx->tfun = h_parse_start(dnp3_p_transport_function);
    assert(ctx->tfun != NULL);
    ctx->tfun_pos = 0;
}

// allocates up to CTXMAX contexts, or recycles the least recently used
struct Context *lookup_context(uint16_t src, uint16_t dst)
{
    struct Context **pnext;
    struct Context *ctx;
    int n=0; // number of context entries

    for(pnext=&contexts; (ctx = *pnext); pnext=&ctx->next) {
        if(ctx->src == src && ctx->dst == dst) {
            *pnext = ctx->next;     // unlink
            ctx->next = contexts;   // move to front of list
            contexts = ctx;

            return ctx;
        }

        n++;
        if(n >= CTXMAX) {
            debug("reuse context %d\n", n);
            break;  // don't advance pnext or ctx
        }
    }
    // ctx points to the last context in list, or NULL
    // *pnext is the next pointer pointing to ctx

    if(!ctx) {
        // allocate a new context
        debug("alloc context %d\n", n+1);
        ctx = calloc(1, sizeof(struct Context));
    } else {
        *pnext = ctx->next; // unlink
    }

    // fill context and place it in front of list
    if(ctx) {
        if(ctx->n > 0) {
            error("L: overflow, %u to %u dropped with %zu bytes!\n",
                  (unsigned)ctx->src, (unsigned)ctx->dst, ctx->n);
        }

        ctx->n = 0;
        ctx->last_segment = NULL;
        reset_tfun(ctx);

        ctx->next = contexts;
        ctx->src = src;
        ctx->dst = dst;
        contexts = ctx;
    }

    return ctx;
}

void free_contexts(void)
{
    struct Context *p = contexts;

    while(p) {
        struct Context *ctx = p;
        p = p->next;
        free(ctx);
    }

    contexts = NULL;
}

void process_transport_segment(struct Context *ctx, const DNP3_Segment *segment)
{
    uint8_t buf[2];
    const DNP3_Segment *tok[2];
    size_t n;

    print("T> %s\n", dnp3_format_segment(segment));

    // convert to input tokens for transport function
    n = transport_tokens(segment, ctx->last_segment, buf, tok);
    ctx->last_segment = segment;
    debug("tfun input: ");
    for(size_t i=0; i<n; i++)
        debug("%c", buf[i]);
    debug("\n");

    // run transport function
    if(!ctx->tfun)
        init_tfun(ctx);
    ttok_values = tok;
    ttok_pos = ctx->tfun_pos;
    bool tfun_done = h_parse_chunk(ctx->tfun, buf, n);
    ttok_values = NULL;
    ctx->tfun_pos += n;
    if(tfun_done)
        reset_tfun(ctx);
}

void process_link_frame(const DNP3_Frame *frame, uint8_t *buf, size_t len)
{
    struct Context *ctx;
    HParseResult *r;

    // always print out the packet
    print("L> %s\n", dnp3_format_frame(frame));

    // payload handling
    switch(frame->func) {
    case DNP3_UNCONFIRMED_USER_DATA:
        if(!frame->payload) // CRC error
            break;

        // look up connection context by source-dest pair
        ctx = lookup_context(frame->source, frame->destination);
        if(!ctx) {
            error("L: connection context failed to allocate\n");
            break;
        }

        // parse and process payload as transport segment
        r = h_parse(dnp3_p_transport_segment, frame->payload, frame->len);
        if(!r) {
            // NB: this should only happen when frame->len = 0, which is
            //     not valid with USER_DATA as per AN2013-004b
            error("T: no parse\n");
            break;
        }

        // append the raw frame to context buf
        if(ctx->n + len <= BUFLEN) {
            memcpy(ctx->buf + ctx->n, buf, len);
            ctx->n += len;
        } else {
            error("T: overflow at %zu bytes,"
                  " dropping %zu byte frame\n", ctx->n, len);
        }

        assert(r->ast);
        process_transport_segment(ctx, H_CAST(DNP3_Segment, r->ast));
        break;
    case DNP3_CONFIRMED_USER_DATA:
        if(!frame->payload) // CRC error
            break;

        error("L: confirmed user data not supported\n");  // XXX ?
        break;
    }
}


/// plugin API ///

// XXX debug
void *h_pprint_lr_info(FILE *f, HParser *p);
void h_pprint_lrtable(FILE *f, void *, void *, int);

int dnp3_printer_init(const Option *opts)
{
    init();

    // XXX debug
#if 0
    void *g = h_pprint_lr_info(stdout, dnp3_p_transport_function);
    assert(g != NULL);
    fprintf(stdout, "\n==== L A L R  T A B L E ====\n");
    h_pprint_lrtable(stdout, g, dnp3_p_transport_function->backend_data, 0);
#endif

    return 0;
}

static int dnp3_printer_feed(Plugin *self, size_t n)
{
    HParseResult *r;
    size_t m=0;

    // parse and process link layer frames
    while((r = h_parse(dnp3_p_synced_frame, self->buf+m, n-m))) {
        size_t consumed = r->bit_length/8;
        assert(r->bit_length%8 == 0);
        assert(consumed > 0);
        assert(r->ast);

        process_link_frame(H_CAST(DNP3_Frame, r->ast), self->buf+m, consumed);

        m += consumed;
    }

    // flush consumed input
    n -= m;
    memmove(buf, self->buf+m, n);
    self->buf = buf + n;
    self->bufsize = BUFLEN - n;

    return 0;
}

static int dnp3_printer_finish(Plugin *self)
{
    free_contexts();
    return 0;
}

Plugin *dnp3_printer(LogCallback log, OutputCallback output, void *env)
{
    static Plugin plugin = {
        .buf = buf,
        .bufsize = BUFLEN,
        .feed = dnp3_printer_feed,
        .finish = dnp3_printer_finish
    };
    static int already = 0;

    if(already)
        return NULL;

    already = 1;
    cb_log = log;
    cb_out = output;
    cb_env = env;
    return &plugin;
}


/// main ///

static void log_stderr(void *env, int priority, const char *fmt, va_list args)
{
    vfprintf(stderr, fmt, args);
}

static void file_write(void *env, const uint8_t *buf, size_t n)
{
    fwrite(buf, 1, n, (FILE *)env);
}

int main(int argc, char *argv[])
{
    Plugin *plugin;

    if(dnp3_printer_init(NULL) < 0) {
        fprintf(stderr, "plugin init failed\n");
        return 1;
    }

    plugin = dnp3_printer(log_stderr, file_write, stdout);
    if(plugin == NULL) {
        fprintf(stderr, "plugin bind failed\n");
        return 1;
    }

    // while stdin open, read input into buf and process
    size_t n;
    while((n=read(0, plugin->buf, plugin->bufsize))) {
        // handle read errors
        if(n<0) {
            if(errno == EINTR)
                continue;
            perror("read");
            return 1;
        }

        if(plugin->feed(plugin, n) < 0) {
            fprintf(stderr, "processing error\n");
            return 1;
        }

        if(plugin->bufsize == 0) {
            fprintf(stderr, "input buffer exhausted\n");
            return 1;
        }
    }

    plugin->finish(plugin);

    return 0;
}
