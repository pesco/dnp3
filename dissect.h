#include "plugin.h"
#include <dnp3.h>

// plugin internals

extern QueueOutputCallback cb_out;
extern void *cb_env;

// dnp3 processing hooks to be provided per plugin
void hook_link_frame(const DNP3_Frame *frame);
void hook_transport_reject(void);
void hook_transport_segment(const DNP3_Segment *segment);
void hook_transport_payload(const uint8_t *s, size_t n);
void hook_app_reject(void);
void hook_app_error(DNP3_ParseError e);
void hook_app_fragment(const DNP3_Fragment *fragment);
