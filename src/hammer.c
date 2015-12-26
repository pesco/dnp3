#include <hammer/hammer.h>
#include <hammer/glue.h>
#include <assert.h>
#include "hammer.h"
#include "sloballoc.h"

static HParsedToken *act_unit(const HParseResult *p, void *tok_)
{
    return (const HParsedToken *)tok_;  // XXX casting away the const?
}
HParser *h_unit(const HParsedToken *tok)
{
    return h_action(h_epsilon_p(), act_unit, (void *)tok);
}
HParser *h_unit__m(HAllocator *mm__, const HParsedToken *tok)
{
    return h_action__m(mm__, h_epsilon_p__m(mm__), act_unit, (void *)tok);
}

static HParsedToken *act_error(const HParseResult *p, void *user)
{
    int code = (intptr_t)user;
    HParsedToken *tok = h_arena_malloc(p->arena, sizeof(HParsedToken));
    tok->token_type = code;
    return tok;
}

HParser *h_error(int code)
{
    assert(H_ISERR(code));

    // could implement in terms of h_unit, but would need an alloc
    return h_action(h_epsilon_p(), act_error, (void *)(intptr_t)code);
}

// helper not officially exported by hammer, but I know it is ;)
HParsedToken *h_make_(HArena *arena, HTokenType type);

HParsedToken *h_make_err(HArena *arena, HTokenType type, void *value)
{
    assert(H_ISERR(type));

    HParsedToken *ret = h_make_(arena, type);
    ret->user = value;
    return ret;
}

HParsedToken *h_make_err_uint(HArena *arena, HTokenType type, uint64_t value)
{
    assert(H_ISERR(type));

    HParsedToken *ret = h_make_(arena, type);
    ret->uint = value;
    return ret;
}

static HParsedToken *act_float(const HParseResult *p, void *user)
{
    HParsedToken *tok = H_ALLOC(HParsedToken);
    tok->token_type = TT_FLOAT;
    tok->dbl = *(float *)&H_CAST_UINT(p->ast);
        // XXX act_float assumes compatible endianness for casting from uint64_t to float
    return tok;
}

static HParsedToken *act_double(const HParseResult *p, void *user)
{
    HParsedToken *tok = H_ALLOC(HParsedToken);
    tok->token_type = TT_FLOAT;
    tok->dbl = *(double *)&H_CAST_UINT(p->ast);
        // XXX act_double assumes compatible endianness for casting from uint64_t to double
    return tok;
}

HParser *h_float32(void)
{
    return h_action(h_uint32(), act_float, NULL);
}

HParser *h_float64(void)
{
    return h_action(h_uint64(), act_double, NULL);
}

static void *h_slob_alloc(HAllocator *mm, size_t size)
{
    SLOB *slob = (SLOB *)(mm+1);
    return sloballoc(slob, size);
}

static void h_slob_free(HAllocator *mm, void *p)
{
    SLOB *slob = (SLOB *)(mm+1);
    slobfree(slob, p);
}

static void *h_slob_realloc(HAllocator *mm, void *p, size_t size)
{
    SLOB *slob = (SLOB *)(mm+1);

    assert(((void)"XXX need realloc for SLOB allocator", 0));
    return NULL;
}

HAllocator *h_sloballoc(void *mem, size_t size)
{
    if(size < sizeof(HAllocator))
        return NULL;

    HAllocator *mm = mem;
    SLOB *slob = slobinit(mem + sizeof(HAllocator), size - sizeof(HAllocator));
    if(!slob)
        return NULL;
    assert(slob == (SLOB *)(mm+1));

    mm->alloc = h_slob_alloc;
    mm->realloc = h_slob_realloc;
    mm->free = h_slob_free;

    return mm;
}
