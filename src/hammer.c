#include <hammer/hammer.h>
#include <hammer/glue.h>
#include <assert.h>
#include "hammer.h"

static HParsedToken *act_unit(const HParseResult *p, void *tok_)
{
    return (const HParsedToken *)tok_;  // XXX casting away the const?
}
HParser *h_unit(const HParsedToken *p)
{
    return h_action(h_epsilon_p(), act_unit, (void *)p);
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
