#include <hammer/hammer.h>
#include <assert.h>

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
    assert(code >= TT_ERR);
    assert(code < TT_USER);

    // could implement in terms of h_unit, but would need an alloc
    return h_action(h_epsilon_p(), act_error, (void *)(intptr_t)code);
}
