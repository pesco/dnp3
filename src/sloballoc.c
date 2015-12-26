// first-fit SLOB (simple list of blocks) allocator

#include "sloballoc.h"
#include <stdint.h>
#include <assert.h>

struct alloc {
    size_t size;
    uint8_t data[];
};

struct block {
    struct alloc alloc;
    struct block *next;
};

struct slob {
    size_t size;
    struct block *head;
    uint8_t data[];
};


SLOB *slobinit(void *mem, size_t size)
{
    SLOB *slob = mem;

    assert(size >= sizeof(SLOB) + sizeof(struct block));
    assert(size < UINTPTR_MAX - (uintptr_t)mem);

    slob = mem;
    slob->size = size - sizeof(SLOB);
    slob->head = mem + sizeof(SLOB);
    slob->head->alloc.size = slob->size - sizeof(struct alloc);
    slob->head->next = NULL;

    return slob;
}

void *sloballoc(SLOB *slob, size_t size)
{
    struct block *b, **p;
    size_t fitblock = size + sizeof(struct block);

    if(fitblock < size) return NULL;    // overflow

    // scan list for the first block of sufficient size
    for(p=&slob->head; b=*p; p=&b->next) {
        if(b->alloc.size >= fitblock) {
            // cut from the end of the block
            b->alloc.size -= sizeof(struct alloc) + size;
            struct alloc *a = (void *)b->alloc.data + b->alloc.size;
            a->size = size;
            return a->data;
        } else if(b->alloc.size >= size) {
            // when a block fills, it converts directly to a struct alloc
            *p = b->next;       // unlink
            return b->alloc.data;
        }
    }

    return NULL;
}

void slobfree(SLOB *slob, void *a_)
{
    struct alloc *a = a_ - sizeof(struct alloc);
    struct block *b, **p, *left=NULL, *right=NULL, **rightp;

    // sanity check: a lies inside slob
    assert((void *)a >= (void *)slob->data);
    assert((void *)a->data + a->size <= (void *)slob->data + slob->size);

    // scan list for blocks adjacent to a
    for(p=&slob->head; b=*p; p=&b->next) {
        if((void *)a == b->alloc.data + b->alloc.size) {
            assert(!left);
            left = b;
        }
        if((void *)a->data + a->size == b) {
            assert(!right);
            right = b;
            rightp = p;
        }

        if(left && right) {
            // extend left and unlink right
            left->alloc.size += sizeof(*a) + a->size +
                                sizeof(right->alloc) + right->alloc.size;
            *rightp = right->next;
            return;
        }
    }

    if(left) {
        // extend left to absorb a
        left->alloc.size += sizeof(*a) + a->size;
    } else if(right) {
        // shift and extend right to absorb a
        right->alloc.size += sizeof(*a) + a->size;
        *rightp = (struct block *)a; **rightp = *right;
    } else {
        // spawn new block over a
        struct block *b = (struct block *)a;
        b->next = slob->head; slob->head = b;
    }
}
