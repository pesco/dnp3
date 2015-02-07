#include <dnp3.h>
#include <hammer/glue.h>
#include "app.h"
#include "util.h"

#include "binary.h"


HParser *dnp3_p_bin_packed;
HParser *dnp3_p_bin_flags;

static HParsedToken *act_packed(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    o->bit = H_CAST_UINT(p->ast);
    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_flags(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    o->flags.online          = H_FIELD_UINT(0);
    o->flags.restart         = H_FIELD_UINT(1);
    o->flags.comm_lost       = H_FIELD_UINT(2);
    o->flags.remote_forced   = H_FIELD_UINT(3);
    o->flags.local_forced    = H_FIELD_UINT(4);
    o->flags.chatter_filter  = H_FIELD_UINT(5);
    o->flags.state           = H_FIELD_UINT(6);

    return H_MAKE(DNP3_Object, o);
}

void dnp3_p_init_binary(void)
{
    H_RULE (bit,         h_bits(1, false));
    H_RULE (reserved,    dnp3_p_reserved(1));

    H_ARULE(packed,      bit);
    H_ARULE(flags,       h_sequence(bit,    // ONLINE
                                    bit,    // RESTART
                                    bit,    // COMM_LOST
                                    bit,    // REMOTE_FORCED
                                    bit,    // LOCAL_FORCED
                                    bit,    // CHATTER_FILTER
                                    reserved,
                                    bit,    // STATE
                                    NULL));

    dnp3_p_bin_packed = packed;
    dnp3_p_bin_flags  = flags;
}
