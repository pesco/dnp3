#include <dnp3.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "dissect.h"


/// hooks ///

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

void hook_link_frame(const DNP3_Frame *frame)
{
    // always print out the packet
    print("L> %s\n", dnp3_format_frame(frame));
}

void hook_transport_reject(void)
{
    print("T: no parse\n");
}

void hook_transport_segment(const DNP3_Segment *segment)
{
    print("T> %s\n", dnp3_format_segment(segment));
}

void hook_transport_payload(const uint8_t *s, size_t n)
{
    print("T: reassembled payload:");
    for(size_t i=0; i<n; i++)
        print(" %.2X", (unsigned int)s[i]);
    print("\n");
}

void hook_app_reject(void)
{
    print("A: no parse\n");
}

// helper
static const char *errorname(DNP3_ParseError e)
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

void hook_app_error(DNP3_ParseError e)
{
    print("A: error %s\n", errorname(e));
}

void hook_app_fragment(const DNP3_Fragment *fragment)
{
    print("A> %s\n", dnp3_format_fragment(fragment));
}
