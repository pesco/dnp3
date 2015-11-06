
#ifndef PLUGIN_H_SEEN
#define PLUGIN_H_SEEN

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

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

typedef void (*QueueOutputCallback)(void *env, const uint8_t *buf, size_t n);

// create plugin instance bound to the given callbacks
Plugin *dnp3_dissect(QueueOutputCallback output, void *env);


// logging hooks to be provided by main program
void error(const char *fmt, ...);
void debug(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif

