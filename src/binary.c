#include <dnp3.h>
#include <hammer/glue.h>
#include "app.h"
#include "util.h"

#include "binary.h"


HParser *dnp3_p_bin_packed;
HParser *dnp3_p_bin_packed2;
HParser *dnp3_p_bin_flags;
HParser *dnp3_p_bin_flags2;

HParser *dnp3_p_abstime;
HParser *dnp3_p_reltime;

static HParsedToken *act_packed(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    o->bit = H_CAST_UINT(p->ast);
    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_packed2(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    o->dblbit = H_CAST_UINT(p->ast);
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

#define act_flags2 act_flags

void dnp3_p_init_binary(void)
{
    H_RULE (bit,         h_bits(1, false));
    H_RULE (dblbit,      h_bits(2, false));
    H_RULE (reserved,    dnp3_p_reserved(1));

    H_ARULE(packed,     bit);
    H_ARULE(packed2,    dblbit);
    H_ARULE(flags,      h_sequence(bit,    // ONLINE
                                   bit,    // RESTART
                                   bit,    // COMM_LOST
                                   bit,    // REMOTE_FORCED
                                   bit,    // LOCAL_FORCED
                                   bit,    // CHATTER_FILTER
                                   reserved,
                                   bit,    // STATE
                                   NULL));

    H_ARULE(flags2,     h_sequence(bit,    // ONLINE
                                   bit,    // RESTART
                                   bit,    // COMM_LOST
                                   bit,    // REMOTE_FORCED
                                   bit,    // LOCAL_FORCED
                                   bit,    // CHATTER_FILTER
                                   dblbit, // STATE
                                   NULL));

    dnp3_p_bin_packed  = packed;
    dnp3_p_bin_packed2 = packed2;
    dnp3_p_bin_flags   = h_with_endianness(BIT_LITTLE_ENDIAN, flags);
    dnp3_p_bin_flags2  = h_with_endianness(BIT_LITTLE_ENDIAN, flags2);
        // XXX switch to bit-little-endian for oblocks in general?!

    // XXX move the time parsers into another file
    dnp3_p_abstime = h_with_endianness(BIT_LITTLE_ENDIAN, h_bits(48, false));
    dnp3_p_reltime = h_with_endianness(BIT_LITTLE_ENDIAN, h_bits(16, false));
        // XXX switch to bit-little-endian for oblocks in general?!
}
