
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <dnp3hammer/dnp3.h>
#include <dnp3hammer/dissect.h>


/// hooks ///

static void print(DissectPlugin *self, const char *fmt, ...)
{
    va_list args;
    char buf[1024];
    int n;

    va_start(args, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if(n >= 0)
        self->out(self->env, (uint8_t *)buf, n);
    else
        error("vsnprintf: %s", strerror(errno));
}

void hook_link_frame(DissectPlugin *self,
                     const DNP3_Frame *frame, const uint8_t *buf, size_t len)
{
    // always print out the packet
    print(self, "L> %s\n", dnp3_format_frame(frame));
}

void hook_transport_reject(DissectPlugin *self)
{
    print(self, "T: no parse\n");
}

void hook_transport_segment(DissectPlugin *self, const DNP3_Segment *segment)
{
    print(self, "T> %s\n", dnp3_format_segment(segment));
}

void hook_transport_payload(DissectPlugin *self, const uint8_t *s, size_t n)
{
    print(self, "T: reassembled payload:");
    for(size_t i=0; i<n; i++)
        print(self, " %.2X", (unsigned int)s[i]);
    print(self, "\n");
}

void hook_app_reject(DissectPlugin *self)
{
    print(self, "A: no parse\n");
}

// helper
static const char *errorname(DNP3_ParseError e)
{
    switch(e) {
    case ERR_FUNC_NOT_SUPP: return "FUNC_NOT_SUPP";
    case ERR_OBJ_UNKNOWN:   return "OBJ_UNKNOWN";
    case ERR_PARAM_ERROR:   return "PARAM_ERROR";
    }

    return "???";
}

void hook_app_error(DissectPlugin *self, DNP3_ParseError e)
{
    print(self, "A: error %s (%d)\n", errorname(e), (int)e);
}

void hook_app_fragment(DissectPlugin *self, const DNP3_Fragment *fragment,
                       const uint8_t *buf, size_t len)
{
    print(self, "A> %s\n", dnp3_format_fragment(fragment));
}
