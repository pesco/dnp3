#include <syslog.h>
#include <stdarg.h>

typedef struct Plugin_ Plugin;
struct Plugin_ {
    // input buffer, pre-allocated, may be altered by feed()
    uint8_t *buf;
    size_t bufsize;

    // return 0 on success, < 0 on error
    int (*feed)(Plugin *self, size_t n);    // consumes either 0 or n bytes
    int (*finish)(Plugin *self);            // invalidates (frees) self
};

typedef struct {
    const char *key;
    const char *value;
} Option;

// one-time global init, returns < 0 on error, opts may be NULL
int dnp3_dissect_init(const Option *opts);

typedef void (*OutputCallback)(void *env, const uint8_t *buf, size_t n);

// create plugin instance bound to the given callbacks
Plugin *dnp3_dissect(OutputCallback output, void *env);


// dnp3 processing hooks to be provided
void hook_link_frame(const DNP3_Frame *frame);
void hook_transport_reject(void);
void hook_transport_segment(const DNP3_Segment *segment);
void hook_transport_payload(const uint8_t *s, size_t n);
void hook_app_reject(void);
void hook_app_error(DNP3_ParseError e);
void hook_app_fragment(const DNP3_Fragment *fragment);

// logging hooks to be provided by main program
void error(const char *fmt, ...);
void debug_(const char *fmt, ...);

#define debug(...) debug_(__VA_ARGS__)  // undefine to disable
