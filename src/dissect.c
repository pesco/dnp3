#include <dnp3hammer.h>
#include <hammer/hammer.h>
#include <hammer/glue.h>
#include "hammer.h"

#include <string.h>
#include <stdlib.h>


#define BUFLEN 4619 // enough for 4096B over 1 frame or 355 empty segments
#define CTXMAX 1024 // maximum number of connection contexts
#define TBUFLEN (BUFLEN/13*2)   // 13 = min. size of a frame
                                // 2  = max. number of tokens per frame


// internal data structures

struct Context {
    struct Context *next;

    uint16_t src;
    uint16_t dst;

    // transport function
    DNP3_Segment last_segment;
    uint8_t last_segment_payload[249];  // max size
    HSuspendedParser *tfun;
    size_t tfun_pos;        // number of bytes consumed so far

    // raw valid frames
    uint8_t buf[BUFLEN];
    size_t n;
};

typedef struct {
    StreamProcessor base;
    uint8_t buf[BUFLEN];        // static input buffer
    struct Context *contexts;   // linked list

    // callbacks
    DNP3_Callbacks cb;
    void *env;
} Dissector;


// high-level parsers  XXX should these be exported?!
HParser *dnp3_p_synced_frame;       // skips bytes until valid frame header
HParser *dnp3_p_transport_function; // the transport-layer state machine


// shorthand to be used in a function foo(Dissector *self, ...)
#define CALLBACK(NAME, ...) \
    do {if(self->cb.NAME) self->cb.NAME(self->env, __VA_ARGS__);} while(0)

#define error(...) CALLBACK(log_error, __VA_ARGS__)
#define debug(...) //fprintf(stderr, __VA_ARGS__)


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
//      (A+[+=]*(Z|[^AZ+=]|$)|[^A])*
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


// helper: create a copy of segment in the given arena
static DNP3_Segment *copy_segment(HArena *arena, const DNP3_Segment *segment)
{
    if(!segment)
        return NULL;

    DNP3_Segment *copy = h_arena_malloc(arena, sizeof(*segment));
    assert(copy != NULL);

    *copy = *segment;
    copy->payload = h_arena_malloc(arena, copy->len);
    assert(copy->payload != NULL);
    memcpy(copy->payload, segment->payload, copy->len);

    return copy;
}

// XXX NOT THREAD-SAFE
static const DNP3_Segment **ttok_values;
static size_t ttok_pos = 0;
static HParsedToken *act_ttok(const HParseResult *p, void *user)
{
    assert(ttok_values != NULL);

    if(!p->ast)
        return NULL;

    debug("tok index %zu, ttok_pos=%zu\n", p->ast->index, ttok_pos);
    assert(p->ast->index >= ttok_pos);
    const DNP3_Segment *v = ttok_values[p->ast->index - ttok_pos];

    return H_MAKE(DNP3_Segment, copy_segment(p->arena, v));
}
static HParser *ttok(const HParser *p)
{
    return h_action(p, act_ttok, NULL);
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

    return H_MAKE_BYTES(t, len);
}

void dnp3_dissector_init(void)
{
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

    // single-step transport function (call repeatedly):
    //
    //     A+[+=]*(Z|[^AZ+=]|$)|[^A]
    //
    H_RULE (pe,      h_many(h_choice(pls, h_ignore(equ), NULL)));
    H_RULE (eof,     h_end_p());
    H_RULE (end,     h_choice(Z, h_ignore(notAZpe), eof, NULL));
    H_RULE (A1,         h_indirect());
    h_bind_indirect(A1, h_choice(h_right(A, A1), A, NULL));
    H_ARULE(series,  h_sequence(A1, pe, end, NULL));
    H_RULE (tfun,    h_choice(series, h_ignore(notA), NULL));

    int tfun_compile = !h_compile(tfun, PB_LALR, NULL);
    assert(tfun_compile);

    dnp3_p_synced_frame = sync;
    dnp3_p_transport_function = tfun;
}


static void reset_tfun(struct Context *ctx)
{
    debug("tfun reset\n");
    if(ctx->tfun) {
        HParseResult *r = h_parse_finish(ctx->tfun);
        assert(r != NULL);
        assert(r->ast == NULL); // no stuck results
        h_parse_result_free(r);
        ctx->tfun = NULL;
    }
}

static void init_tfun(struct Context *ctx)
{
    debug("tfun init\n");
    assert(ctx->tfun == NULL);
    ctx->tfun = h_parse_start(dnp3_p_transport_function);
    assert(ctx->tfun != NULL);
    ctx->tfun_pos = 0;
}

static
HParseResult *feed_tfun(struct Context *ctx, const uint8_t *buf,
                        const DNP3_Segment **tok, size_t offs, size_t n)
{
    HParseResult *r = NULL;
    debug("feed_tfun(...,%zu,%zu)\n", offs, n);

    if(!ctx->tfun)
        init_tfun(ctx);

    ttok_values = tok + offs;
    ttok_pos = ctx->tfun_pos + offs;
    bool done = h_parse_chunk(ctx->tfun, buf + offs, n - offs);
    ttok_values = NULL;

    if(done) {
        r = h_parse_finish(ctx->tfun);
        assert(r != NULL);  // tfun always accepts
        ctx->tfun = NULL;
    }

    return r;
}

// allocates up to CTXMAX contexts, or recycles the least recently used
static
struct Context *lookup_context(Dissector *self, uint16_t src, uint16_t dst)
{
    struct Context **pnext;
    struct Context *ctx;
    int n=0; // number of context entries

    for(pnext=&self->contexts; (ctx = *pnext); pnext=&ctx->next) {
        if(ctx->src == src && ctx->dst == dst) {
            *pnext = ctx->next;         // unlink
            ctx->next = self->contexts; // move to front of list
            self->contexts = ctx;

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
            error("context overflow, %u to %u dropped with %zu bytes!\n",
                  (unsigned)ctx->src, (unsigned)ctx->dst, ctx->n);
        }

        ctx->n = 0;
        reset_tfun(ctx);

        ctx->next = self->contexts;
        ctx->src = src;
        ctx->dst = dst;
        self->contexts = ctx;
    }

    return ctx;
}

static
void process_transport_payload(Dissector *self, struct Context *ctx,
                               const uint8_t *t, size_t len)
{
    CALLBACK(transport_payload, t, len);

    // try to parse a message fragment
    HParseResult *r = h_parse(dnp3_p_app_fragment, t, len);
    if(r) {
        assert(r->ast != NULL);
        if(H_ISERR(r->ast->token_type)) {
            CALLBACK(app_invalid, r->ast->token_type);
        } else {
            DNP3_Fragment *fragment = H_CAST(DNP3_Fragment, r->ast);
            CALLBACK(app_fragment, fragment, ctx->buf, ctx->n);
        }
        h_parse_result_free(r);
    } else {
        CALLBACK(app_invalid, 0);
    }

    ctx->n = 0; // flush frames
}

// helper
static void save_last_segment(struct Context *ctx, const DNP3_Segment *segment)
{
    assert(segment->len <= sizeof(ctx->last_segment_payload));
    memcpy(ctx->last_segment_payload, segment->payload, segment->len);
    ctx->last_segment = *segment;
    ctx->last_segment.payload = ctx->last_segment_payload;
}

static
void process_transport_segment(Dissector *self,
                               struct Context *ctx, const DNP3_Segment *segment)
{
    uint8_t buf[2];
    const DNP3_Segment *tok[2];
    size_t n;
    HParseResult *r;

    CALLBACK(transport_segment, segment);

    // convert to input tokens for transport function
    n = transport_tokens(segment, &ctx->last_segment, buf, tok);
    save_last_segment(ctx, segment);
    debug("tfun input: ");
    for(size_t i=0; i<n; i++)
        debug("%c", buf[i]);
    debug("\n");

    // run transport function
    size_t m=0;
    while(m<n && (r = feed_tfun(ctx, buf, tok, m, n))) {
        assert(r->bit_length%8 == 0);
        size_t consumed = r->bit_length/8 - ctx->tfun_pos;
        assert(consumed > 0);

        // process reassembled segment series if any
        if(r->ast) {
            HBytes b = H_CAST_BYTES(r->ast);
            process_transport_payload(self, ctx, b.token, b.len);
            h_parse_result_free(r);
        }
        ctx->n = 0; // flush frames (XXX => drop invalid series - OK?)

        m += consumed;
    }

    ctx->tfun_pos += n;
}

static
void process_link_frame(Dissector *self,
                        const DNP3_Frame *frame, uint8_t *buf, size_t len)
{
    struct Context *ctx;
    HParseResult *r;

    if(!dnp3_link_validate_frame(frame)) {
        CALLBACK(link_invalid, frame);
        return;
    }

    CALLBACK(link_frame, frame, buf, len);

    // payload handling
    switch(frame->func) {
    case DNP3_UNCONFIRMED_USER_DATA:
        if(!frame->payload) // CRC error
            break;

        // look up connection context by source-dest pair
        ctx = lookup_context(self, frame->source, frame->destination);
        if(!ctx) {
            error("connection context failed to allocate\n");
            break;
        }

        // parse and process payload as transport segment
        r = h_parse(dnp3_p_transport_segment, frame->payload, frame->len);
        if(!r) {
            // NB: this should only happen when frame->len = 0, which is
            //     not valid with USER_DATA as per AN2013-004b
            break;
        }

        // append the raw frame to context buf
        if(ctx->n + len <= BUFLEN) {
            memcpy(ctx->buf + ctx->n, buf, len);
            ctx->n += len;
        } else {
            error("overflow at %zu bytes, dropping %zu byte frame\n",
                  ctx->n, len);
        }

        assert(r->ast);
        process_transport_segment(self, ctx, H_CAST(DNP3_Segment, r->ast));
        h_parse_result_free(r);
        break;
    case DNP3_CONFIRMED_USER_DATA:
        if(!frame->payload) // CRC error
            break;

        error("confirmed user data not supported\n");  // XXX ?
        break;
    }
}

static int dissector_feed(StreamProcessor *base, size_t n)
{
    Dissector *self = (Dissector *)base;
    HParseResult *r;
    size_t m=0;

    // parse and process link layer frames
    while((r = h_parse(dnp3_p_synced_frame, base->buf+m, n-m))) {
        size_t consumed = r->bit_length/8;
        assert(r->bit_length%8 == 0);
        assert(consumed > 0);
        assert(r->ast);

        process_link_frame(self, H_CAST(DNP3_Frame, r->ast),
                           base->buf+m, consumed);
        h_parse_result_free(r);

        m += consumed;
    }

    // flush consumed input
    n -= m;
    memmove(self->buf, base->buf+m, n);
    base->buf = self->buf + n;
    base->bufsize = BUFLEN - n;

    return 0;
}

static int dissector_finish(StreamProcessor *base)
{
    Dissector *self = (Dissector *)base;

    // free contexts
    struct Context *p;
    while((p = self->contexts)) {
        self->contexts = p->next;
        free(p);
    }

    free(self);
    return 0;
}

StreamProcessor *dnp3_dissector(DNP3_Callbacks cb, void *env)
{
    Dissector *p = malloc(sizeof(Dissector));

    if(p) {
        p->base.buf     = p->buf;
        p->base.bufsize = sizeof(p->buf);
        p->base.feed    = dissector_feed;
        p->base.finish  = dissector_finish;
        p->contexts     = NULL;
        p->cb           = cb;
        p->env          = env;
    }

    assert((StreamProcessor *)p == &p->base);
    return &p->base;
}
