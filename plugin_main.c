#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include "dissect.h"


/// main ///

void error(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fputs("error: ", stderr);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void debug_(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

static void file_write(void *env, const uint8_t *buf, size_t n)
{
    fwrite(buf, 1, n, (FILE *)env);
}

int main(int argc, char *argv[])
{
    Plugin *plugin;

    if(dnp3_dissect_init(NULL) < 0) {
        fprintf(stderr, "plugin init failed\n");
        return 1;
    }

    plugin = dnp3_dissect(file_write, stdout);
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
