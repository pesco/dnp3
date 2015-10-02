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
int dnp3_printer_init(const Option *opts);

typedef void (*LogCallback)(void *env, int priority, const char *fmt, va_list args);
typedef void (*OutputCallback)(void *env, const uint8_t *buf, size_t n);

// create plugin instance bound to the given callbacks
Plugin *dnp3_printer(LogCallback log, OutputCallback output, void *env);
