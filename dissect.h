#include "plugin.h"
#include <dnp3.h>

// plugin internals

#define BUFLEN 4619 // enough for 4096B over 1 frame or 355 empty segments
#define CTXMAX 1024 // maximum number of connection contexts
#define TBUFLEN (BUFLEN/13*2)   // 13 = min. size of a frame
                                // 2  = max. number of tokens per frame

struct Context {
    struct Context *next;

    uint16_t src;
    uint16_t dst;

    // transport function
    DNP3_Segment last_segment;
    uint8_t last_segment_payload[249];  // max size
    HSuspendedParser *tfun;
    size_t tfun_pos;        // number of bytes consumed so far

    // raw valid frames
    uint8_t buf[BUFLEN];
    size_t n;
};

typedef struct {
    Plugin base;
    uint8_t buf[BUFLEN];        // static input buffer
    struct Context *contexts;   // linked list

    // callbacks
    QueueOutputCallback out;
    void *env;
} DissectPlugin;


// dnp3 processing hooks to be provided per plugin
void hook_link_frame(DissectPlugin *self,
                     const DNP3_Frame *frame, const uint8_t *buf, size_t len);
void hook_transport_reject(DissectPlugin *self);
void hook_transport_segment(DissectPlugin *self, const DNP3_Segment *segment);
void hook_transport_payload(DissectPlugin *self, const uint8_t *s, size_t n);
void hook_app_reject(DissectPlugin *self);
void hook_app_error(DissectPlugin *self, DNP3_ParseError e);
void hook_app_fragment(DissectPlugin *self, const DNP3_Fragment *fragment,
                       const uint8_t *buf, size_t len);
