#include <dnp3.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "dissect.h"


/// hooks ///

static void pass(DissectPlugin *self, const uint8_t *input, size_t len)
{
    self->out(self->env, input, len);
}

void hook_link_frame(DissectPlugin *self,
                     const DNP3_Frame *frame, const uint8_t *buf, size_t len)
{
    if(frame->func == DNP3_UNCONFIRMED_USER_DATA ||
       frame->func == DNP3_CONFIRMED_USER_DATA) {
        return;
    }

    // pass non-data frames
    pass(self, buf, len);
}

void hook_transport_reject(DissectPlugin *self)
{
    error("transport-layer segment: no parse\n");
}

void hook_transport_segment(DissectPlugin *self, const DNP3_Segment *segment)
{
    // do nothing
}

void hook_transport_payload(DissectPlugin *self, const uint8_t *s, size_t n)
{
    // do nothing
}

void hook_app_reject(DissectPlugin *self)
{
    error("application-layer fragment: no parse\n");
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
    error("application-layer error: %s (%d)\n", errorname(e), (int)e);
}

void hook_app_fragment(DissectPlugin *self, const DNP3_Fragment *fragment,
                       const uint8_t *buf, size_t len)
{
    pass(self, buf, len);
}
