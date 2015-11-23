#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <dnp3hammer/dnp3.h>


/// callbacks ///

static void error(void *env, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fputs("error: ", stderr);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void output_frame(void *env, const DNP3_Frame *frame,
                  const uint8_t *buf, size_t len);
void output_fragment(void *env, const DNP3_Fragment *fragment,
                     const uint8_t *buf, size_t len);


/// main ///

int main(int argc, char *argv[])
{
    StreamProcessor *p;
    DNP3_Callbacks callbacks = {NULL};

    dnp3_init();

    callbacks.link_frame = output_frame;
    callbacks.app_fragment = output_fragment;
    callbacks.log_error = error;

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
