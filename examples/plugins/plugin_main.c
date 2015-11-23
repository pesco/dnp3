#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <dnp3hammer/dnp3.h>


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

void output_ctrl_frame(void *env, const DNP3_Frame *frame,
                       const uint8_t *buf, size_t len)
{
    if(frame->func == DNP3_UNCONFIRMED_USER_DATA ||
       frame->func == DNP3_CONFIRMED_USER_DATA) {
        return;
    }

    output(env, buf, len);
}

void output_fragment(void *env, const DNP3_Fragment *fragment,
                     const uint8_t *buf, size_t len)
{
    output(env, buf, len);
}

void print_frame(void *env, const DNP3_Frame *frame,
                const uint8_t *buf, size_t len)
{
    print(env, "L> %s\n", dnp3_format_frame(frame));
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
    "usage: dissect [-f]\n"
    "    -f  filter: pass valid traffic to stdout\n"
    ;
    
int main(int argc, char *argv[])
{
    StreamProcessor *p;
    DNP3_Callbacks callbacks = {NULL};

    // default: print mode
    callbacks.link_frame = print_frame;
    callbacks.transport_segment = print_segment;
    callbacks.transport_payload = print_transport_payload;
    callbacks.app_invalid = print_app_invalid;
    callbacks.app_fragment = print_fragment;
    callbacks.log_error = error;

    // command line
    int ch;
    while((ch = getopt(argc, argv, "hf")) != -1) {
        switch(ch) {
        case 'f':
            // filter mode
            callbacks.link_frame = output_ctrl_frame;
            callbacks.transport_segment = NULL;
            callbacks.transport_payload = NULL;
            callbacks.app_invalid = NULL;
            callbacks.app_fragment = output_fragment;
            break;
        default:
            fputs(usage, stderr);
            return 1;
        }
    }
    argc -= optind;
    argv += optind;

    dnp3_init();

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
