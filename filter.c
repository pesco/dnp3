#include <dnp3.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "dissect.h"


/// hooks ///

static void pass(const uint8_t *input, size_t len)
{
    cb_out(cb_env, input, len);
}

void hook_link_frame(const DNP3_Frame *frame, const uint8_t *buf, size_t len)
{
    if(frame->func == DNP3_UNCONFIRMED_USER_DATA ||
       frame->func == DNP3_CONFIRMED_USER_DATA) {
        return;
    }

    // pass non-data frames
    pass(buf, len);
}

void hook_transport_reject(void)
{
    error("transport-layer segment: no parse\n");
}

void hook_transport_segment(const DNP3_Segment *segment)
{
    // do nothing
}

void hook_transport_payload(const uint8_t *s, size_t n)
{
    // do nothing
}

void hook_app_reject(void)
{
    error("application-layer fragment: no parse\n");
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
    error("application-layer error: %s\n", errorname(e));
}

void hook_app_fragment(const DNP3_Fragment *fragment,
                       const uint8_t *buf, size_t len)
{
    pass(buf, len);
}
