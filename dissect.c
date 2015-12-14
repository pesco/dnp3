#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <dnp3hammer.h>


/// helpers ///

static void output(void *env, const uint8_t *input, size_t len)
{
    fwrite(input, 1, len, (FILE *)env); // XXX loop; use write(2)?
}

static void print(void *env, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf((FILE *)env, fmt, args);
    va_end(args);
}

static const char *errorname(DNP3_ParseError e)
{
    switch(e) {
    case ERR_FUNC_NOT_SUPP: return "FUNC_NOT_SUPP";
    case ERR_OBJ_UNKNOWN:   return "OBJ_UNKNOWN";
    case ERR_PARAM_ERROR:   return "PARAM_ERROR";
    }
    return "???";
}


/// callback functions ///

static void error(void *env, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fputs("error: ", stderr);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

int output_ctrl_frame(void *env, const DNP3_Frame *frame,
                      const uint8_t *buf, size_t len)
{
    if(frame->func == DNP3_UNCONFIRMED_USER_DATA ||
       frame->func == DNP3_CONFIRMED_USER_DATA) {
        return 0;
    }

    output(env, buf, len);

    return 0;
}

void output_fragment(void *env, const DNP3_Fragment *fragment,
                     const uint8_t *buf, size_t len)
{
    output(env, buf, len);
}

int print_frame(void *env, const DNP3_Frame *frame,
                const uint8_t *buf, size_t len)
{
    print(env, "L> %s\n", dnp3_format_frame(frame));
    return 0;
}

void print_link_invalid(void *env, const DNP3_Frame *frame)
{
    print(env, "L: invalid %s\n", dnp3_format_frame(frame));
}

void print_fragment(void *env, const DNP3_Fragment *fragment,
                     const uint8_t *buf, size_t len)
{
    print(env, "A> %s\n", dnp3_format_fragment(fragment));
}

void print_segment(void *env, const DNP3_Segment *segment)
{
    print(env, "T> %s\n", dnp3_format_segment(segment));
}

void print_transport_discard(void *env, size_t n)
{
    print(env, "T: discarding invalid data (%u bytes)\n", (unsigned int)n);
}
void print_transport_payload(void *env, const uint8_t *s, size_t n)
{
    print(env, "T: reassembled payload:");
    for(size_t i=0; i<n; i++)
        print(env, " %.2X", (unsigned int)s[i]);
    print(env, "\n");
}

void print_app_invalid(void *env, DNP3_ParseError e)
{
    if(!e)
        print(env, "A: no parse\n");
    else
        print(env, "A: error %s (%d)\n", errorname(e), (int)e);
}


/// main ///

const char *usage =
    "usage: dissect [-TAf]\n"
    "    -T  read a single transport segment from stdin\n"
    "    -A  read a single app-layer fragment from stdin\n"
    "    -f  filter: pass valid traffic to stdout\n"
    ;

DNP3_Callbacks callbacks = {NULL};

int main_app(void);
int main_transport(void);
int main_full(void);

int main(int argc, char *argv[])
{
    int (*main_)(void);

    // default: full trafic, print mode
    main_ = main_full;
    callbacks.link_frame = print_frame;
    callbacks.link_invalid = print_link_invalid;
    callbacks.transport_segment = print_segment;
    callbacks.transport_discard = print_transport_discard;
    callbacks.transport_payload = print_transport_payload;
    callbacks.app_invalid = print_app_invalid;
    callbacks.app_fragment = print_fragment;
    callbacks.log_error = error;

    // command line
    int ch;
    while((ch = getopt(argc, argv, "TAfh")) != -1) {
        switch(ch) {
        case 'f': // filter mode
            callbacks.link_frame = output_ctrl_frame;
            callbacks.link_invalid = NULL;
            callbacks.transport_segment = NULL;
            callbacks.transport_discard = NULL;
            callbacks.transport_payload = NULL;
            callbacks.app_invalid = NULL;
            callbacks.app_fragment = output_fragment;
            break;
        case 'A': // app-layer only
            main_ = main_app;
            break;
        case 'T': // transport-layer
            main_ = main_transport;
            break;
        default:
            fputs(usage, stderr);
            return 1;
        }
    }
    argc -= optind;
    argv += optind;

    dnp3_init();
    return main_();
}

int main_full(void)
{
    StreamProcessor *p;

    p = dnp3_dissector(callbacks, stdout);
    if(p == NULL) {
        fprintf(stderr, "protocol init failed\n");
        return 1;
    }

    // while stdin open, read input into buf and process
    size_t n;
    while((n=read(0, p->buf, p->bufsize))) {
        // handle read errors
        if(n<0) {
            if(errno == EINTR)
                continue;
            perror("read");
            return 1;
        }

        if(p->feed(p, n) < 0) {
            fprintf(stderr, "processing error\n");
            return 1;
        }

        if(p->bufsize == 0) {
            fprintf(stderr, "input buffer exhausted\n");
            return 1;
        }
    }

    p->finish(p);
    return 0;
}

#define BUFLEN 4096
#define CALLBACK(NAME, ...) \
    do {if(callbacks.NAME) callbacks.NAME(stdout, __VA_ARGS__);} while(0)

int app_layer(const uint8_t *buf, size_t n, const uint8_t *raw, size_t rawn)
{
    // always exercise both parsers, for test coverage
    HParseResult *request  = h_parse(dnp3_p_app_request, buf, n);
    HParseResult *response = h_parse(dnp3_p_app_response, buf, n);

    HParseResult *result;
    if(request && request->ast->token_type == TT_DNP3_Fragment) {
        assert(!(response && response->ast->token_type == TT_DNP3_Fragment));
        result = request;
    } else {
        result = response;
    }

    if(!result) {
        CALLBACK(app_invalid, 0);
        return 1;
    }

    if(result->ast->token_type == TT_DNP3_Fragment)
        CALLBACK(app_fragment, result->ast->user, raw, rawn);
    else
        CALLBACK(app_invalid, result->ast->token_type);
    h_parse_result_free(result);
    return 0;
}

int main_app(void)
{
    uint8_t buf[BUFLEN];
    size_t n;

    n = fread(buf, 1, BUFLEN, stdin);
    return app_layer(buf, n, buf, n);
}

int main_transport(void)
{
    uint8_t buf[BUFLEN];
    size_t n;

    n = fread(buf, 1, BUFLEN, stdin);
    HParseResult *res = h_parse(dnp3_p_transport_segment, buf, n);
    if(!res)
        return 1;   // shouldn't happen

    DNP3_Segment *segment = res->ast->user;
    CALLBACK(transport_segment, segment);
    if(segment->payload)
        return app_layer(segment->payload, segment->len, buf, n);
    return 0;
}
