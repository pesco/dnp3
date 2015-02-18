#include <dnp3.h>
#include <hammer/glue.h>
#include "../app.h"
#include "../util.h"

#include "binary.h"


HParser *dnp3_p_binin_rblock;
HParser *dnp3_p_binin_oblock;

HParser *dnp3_p_bininev_rblock;
HParser *dnp3_p_bininev_oblock;

HParser *dnp3_p_dblbitin_rblock;
HParser *dnp3_p_dblbitin_oblock;

HParser *dnp3_p_dblbitinev_rblock;
HParser *dnp3_p_dblbitinev_oblock;

HParser *dnp3_p_binout_rblock;
HParser *dnp3_p_binout_oblock;
HParser *dnp3_p_g10v1_binout_packed_oblock;

HParser *dnp3_p_binoutev_rblock;
HParser *dnp3_p_binoutev_oblock;


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

static HParsedToken *act_outflags(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    o->flags.online          = H_FIELD_UINT(0);
    o->flags.restart         = H_FIELD_UINT(1);
    o->flags.comm_lost       = H_FIELD_UINT(2);
    o->flags.remote_forced   = H_FIELD_UINT(3);
    o->flags.local_forced    = H_FIELD_UINT(4);
    o->flags.state           = H_FIELD_UINT(5);

    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_flags_abs(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (flags, abstime)
    o->timed.flags = H_FIELD(DNP3_Object, 0)->flags;
    o->timed.abstime = H_FIELD_UINT(1);

    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_flags_rel(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (flags, reltime)
    o->timed.flags = H_FIELD(DNP3_Object, 0)->flags;
    o->timed.reltime = H_FIELD_UINT(1);

    return H_MAKE(DNP3_Object, o);
}

#define act_flags2_abs act_flags_abs
#define act_flags2_rel act_flags_rel

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
    H_ARULE(outflags,   h_sequence(bit,    // ONLINE
                                   bit,    // RESTART
                                   bit,    // COMM_LOST
                                   bit,    // REMOTE_FORCED
                                   bit,    // LOCAL_FORCED
                                   reserved,
                                   reserved,
                                   bit,    // STATE
                                   NULL));

    H_ARULE(flags_abs,  h_sequence(flags, dnp3_p_dnp3time, NULL));
    H_ARULE(flags_rel,  h_sequence(flags, dnp3_p_reltime, NULL));
    H_ARULE(flags2_abs, h_sequence(flags2, dnp3_p_dnp3time, NULL));
    H_ARULE(flags2_rel, h_sequence(flags2, dnp3_p_reltime, NULL));


    // group 1: binary inputs...
    // XXX does oblock_packed become unnecessary when little-endian is the default?
    H_RULE (oblock_packed,      dnp3_p_oblock_packed(G_V(BININ, PACKED), packed));
    H_RULE (oblock_flags,       dnp3_p_oblock(G_V(BININ, FLAGS), flags));

    dnp3_p_binin_rblock     = dnp3_p_rblock(G(BININ),
                                            V(BININ, PACKED),
                                            V(BININ, FLAGS), 0);
    dnp3_p_binin_oblock     = h_choice(oblock_packed, oblock_flags, NULL);

    // group 2: binary input events...
    H_RULE (oblock_notime,      dnp3_p_oblock(G_V(BININEV, NOTIME), flags));
    H_RULE (oblock_abstime,     dnp3_p_oblock(G_V(BININEV, ABSTIME), flags_abs));
    H_RULE (oblock_reltime,     dnp3_p_oblock(G_V(BININEV, RELTIME), flags_rel));

    dnp3_p_bininev_rblock   = dnp3_p_rblock(G(BININEV),
                                            V(BININEV, NOTIME),
                                            V(BININEV, ABSTIME),
                                            V(BININEV, RELTIME), 0);
    dnp3_p_bininev_oblock   = h_choice(oblock_notime,
                                       oblock_abstime,
                                       oblock_reltime, NULL);

    // group 3: double-bit binary inputs...
    H_RULE (oblock_packed2,     dnp3_p_oblock_packed(G_V(DBLBITIN, PACKED), packed2));
    H_RULE (oblock_flags2,      dnp3_p_oblock(G_V(DBLBITIN, FLAGS), flags2));

    dnp3_p_dblbitin_rblock  = dnp3_p_rblock(G(DBLBITIN),
                                            V(DBLBITIN, PACKED),
                                            V(DBLBITIN, FLAGS), 0);
    dnp3_p_dblbitin_oblock  = h_choice(oblock_packed2, oblock_flags2, NULL);

    // group 4: double-bit binary input events...
    H_RULE (oblock_notime2,     dnp3_p_oblock(G_V(DBLBITINEV, NOTIME), flags2));
    H_RULE (oblock_abstime2,    dnp3_p_oblock(G_V(DBLBITINEV, ABSTIME), flags2_abs));
    H_RULE (oblock_reltime2,    dnp3_p_oblock(G_V(DBLBITINEV, RELTIME), flags2_rel));

    dnp3_p_dblbitinev_rblock = dnp3_p_rblock(G(DBLBITINEV),
                                             V(DBLBITINEV, NOTIME),
                                             V(DBLBITINEV, ABSTIME),
                                             V(DBLBITINEV, RELTIME), 0);
    dnp3_p_dblbitinev_oblock = h_choice(oblock_notime2,
                                        oblock_abstime2,
                                        oblock_reltime2, NULL);

    // group 10: binary outputs...
    H_RULE (oblock_outpacked,   dnp3_p_oblock_packed(G_V(BINOUT, PACKED), packed));
    H_RULE (oblock_outflags,    dnp3_p_oblock(G_V(BINOUT, FLAGS), outflags));

    dnp3_p_binout_rblock    = dnp3_p_rblock(G(BINOUT),
                                            V(BINOUT, PACKED),
                                            V(BINOUT, FLAGS), 0);
    dnp3_p_binout_oblock    = h_choice(oblock_outpacked, oblock_outflags, NULL);
    dnp3_p_g10v1_binout_packed_oblock = oblock_outpacked;

    // group 11: binary output events...
    H_RULE (oblock_outnotime,   dnp3_p_oblock(G_V(BINOUTEV, NOTIME), flags));
    H_RULE (oblock_outabstime,  dnp3_p_oblock(G_V(BINOUTEV, ABSTIME), flags_abs));

    dnp3_p_binoutev_rblock  = dnp3_p_rblock(G(BINOUTEV),
                                            V(BINOUTEV, NOTIME),
                                            V(BINOUTEV, ABSTIME), 0);
    dnp3_p_binoutev_oblock  = h_choice(oblock_outnotime,
                                       oblock_outabstime, NULL);
}
