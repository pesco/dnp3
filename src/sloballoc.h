#ifndef SLOBALLOC_H_SEEN
#define SLOBALLOC_H_SEEN

#include <stddef.h>

typedef struct slob SLOB;

SLOB *slobinit(void *mem, size_t size);
void *sloballoc(SLOB *slob, size_t size);
void slobfree(SLOB *slob, void *p);

// consistency check (verify internal invariants); returns 0 on success
int slobcheck(SLOB *slob);

#endif // SLOBALLOC_H_SEEN
