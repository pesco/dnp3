#include <dnp3.h>
#include <hammer/glue.h>
#include "hammer.h" // XXX placeholder for extensions
#include <assert.h>
#include "util.h"


static bool is_zero(HParseResult *p, void *user)
{
    assert(p->ast != NULL);
    return (H_CAST_UINT(p->ast) == 0);
}

HParser *dnp3_p_reserved(size_t n)
{
    return h_ignore(h_attr_bool(h_bits(n, false), is_zero, NULL));
}

static bool validate_ochoice(HParseResult *p, void *user)
{
    // we identify PARAM_ERROR with parse failure, so if one of the object
    // parsers returns it, just fail the choice.
    return (p->ast && p->ast->token_type != ERR_PARAM_ERROR);
}

HParser *dnp3_p_objchoice(HParser *p, ...)
{
    va_list ap;
    va_start(ap, p);

    // XXX don't generate these anew each call!
    H_RULE(octet,       h_uint8());
    H_RULE(ohdr_,       h_repeat_n(octet, 3));  // (grp,var,qc)
    H_RULE(unk,         h_right(ohdr_, h_error(ERR_OBJ_UNKNOWN)));

    H_RULE (ps,         h_choice__v(p, ap));
    H_VRULE(ochoice,    h_choice(ps, unk, NULL));

    va_end(ap);
    return ochoice;
}

static bool not_err(HParseResult *p, void *user)
{
    return !ISERR(p->ast->token_type);
}

HParser *dnp3_p_many(HParser *p)
{
    H_RULE(p_ok,    h_attr_bool(p, not_err, NULL));

    H_RULE(ps,      h_many(p_ok));
    H_RULE(err,     h_right(ps, p));    // fails or yields error
    H_RULE(many,    h_choice(err, ps, NULL));

    return many;
}

static bool is_err(HParseResult *p, void *user)
{
    return ISERR(p->ast->token_type);
}

HParser *dnp3_p_packet(HParser *p)
{
    H_RULE(err, h_attr_bool(p, is_err, NULL));
    H_RULE(ok,  h_left(p, h_end_p()));
    H_RULE(pkt, h_choice(err, ok, NULL));

    return pkt;
}
