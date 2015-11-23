#include <stdio.h>

#include <dnp3hammer/dnp3.h>


// helper
static void print(void *env, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf((FILE *)env, fmt, args);
    va_end(args);
}

void output_frame(void *env, const DNP3_Frame *frame,
                  const uint8_t *buf, size_t len)
{
    // always print out the packet
    print(env, "L> %s\n", dnp3_format_frame(frame));
}

void output_fragment(void *env, const DNP3_Fragment *fragment,
                     const uint8_t *buf, size_t len)
{
    print(env, "A> %s\n", dnp3_format_fragment(fragment));
}



#if 0

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

#endif
