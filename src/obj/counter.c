#include <dnp3.h>
#include <hammer/glue.h>
#include "../app.h"
#include "../util.h"

#include "counter.h"


HParser *dnp3_p_ctr_rblock;
HParser *dnp3_p_ctr_oblock;
HParser *dnp3_p_frozenctr_rblock;
HParser *dnp3_p_frozenctr_oblock;
HParser *dnp3_p_ctrev_rblock;
HParser *dnp3_p_ctrev_oblock;
HParser *dnp3_p_frozenctrev_rblock;
HParser *dnp3_p_frozenctrev_oblock;


static HParsedToken *act_flags(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    o->flags.online          = H_FIELD_UINT(0);
    o->flags.restart         = H_FIELD_UINT(1);
    o->flags.comm_lost       = H_FIELD_UINT(2);
    o->flags.remote_forced   = H_FIELD_UINT(3);
    o->flags.local_forced    = H_FIELD_UINT(4);
    o->flags.discontinuity   = H_FIELD_UINT(5);

    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_ctr_flag(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (flags, value)
    o->ctr.flags = H_FIELD(DNP3_Object, 0)->flags;
    o->ctr.value = H_FIELD_UINT(1);

    return H_MAKE(DNP3_Object, o);
}

#define act_ctr32_flag act_ctr_flag
#define act_ctr16_flag act_ctr_flag

static HParsedToken *act_ctr_flag_time(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (flags, value)
    o->timed.ctr.flags = H_FIELD(DNP3_Object, 0)->flags;
    o->timed.ctr.value = H_FIELD_UINT(1);
    o->timed.abstime   = H_FIELD_UINT(2);

    return H_MAKE(DNP3_Object, o);
}

#define act_ctr32_flag_t act_ctr_flag_time
#define act_ctr16_flag_t act_ctr_flag_time

static HParsedToken *act_ctr(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    o->ctr.value = H_CAST_UINT(p->ast);
    return H_MAKE(DNP3_Object, o);
}

#define act_ctr32 act_ctr
#define act_ctr16 act_ctr

void dnp3_p_init_counter(void)
{
    H_RULE (bit,        h_bits(1,false));
    H_RULE (ignore,     h_ignore(bit));
    H_RULE (reserved,   dnp3_p_reserved(1));

    H_ARULE(flags,      h_sequence(bit,         // ONLINE
                                   bit,         // RESTART
                                   bit,         // COMM_LOST
                                   bit,         // REMOTE_FORCED
                                   bit,         // LOCAL_FORCED
                                   ignore,      // (ROLLOVER - obsolete)
                                   bit,         // DISCONTINUITY
                                   reserved,
                                   NULL));
    H_RULE (val32,      h_uint32());
    H_RULE (val16,      h_uint16());

    H_ARULE(ctr32,          val32);
    H_ARULE(ctr16,          val16);
    H_ARULE(ctr32_flag,     h_sequence(flags, val32, NULL));
    H_ARULE(ctr16_flag,     h_sequence(flags, val16, NULL));
    H_ARULE(ctr32_flag_t,   h_sequence(flags, val32, dnp3_p_dnp3time, NULL));
    H_ARULE(ctr16_flag_t,   h_sequence(flags, val16, dnp3_p_dnp3time, NULL));

    // group 20: counters...
    H_RULE(oblock_32bit_flag,   dnp3_p_oblock(G_V(CTR, 32BIT_FLAG), ctr32_flag));
    H_RULE(oblock_16bit_flag,   dnp3_p_oblock(G_V(CTR, 16BIT_FLAG), ctr16_flag));
    H_RULE(oblock_32bit_noflag, dnp3_p_oblock(G_V(CTR, 32BIT_NOFLAG), ctr32));
    H_RULE(oblock_16bit_noflag, dnp3_p_oblock(G_V(CTR, 16BIT_NOFLAG), ctr16));

    dnp3_p_ctr_rblock = dnp3_p_rblock(G(CTR), V(CTR, 32BIT_FLAG),
                                              V(CTR, 16BIT_FLAG),
                                              V(CTR, 32BIT_NOFLAG),
                                              V(CTR, 32BIT_NOFLAG), 0);
    dnp3_p_ctr_oblock = h_choice(oblock_32bit_flag,
                                 oblock_16bit_flag,
                                 oblock_32bit_noflag,
                                 oblock_16bit_noflag,
                                 NULL);

    // group 21: frozen counters...
    H_RULE(oblock_frz32bit_flag,   dnp3_p_oblock(G_V(FROZENCTR, 32BIT_FLAG), ctr32_flag));
    H_RULE(oblock_frz16bit_flag,   dnp3_p_oblock(G_V(FROZENCTR, 16BIT_FLAG), ctr16_flag));
    H_RULE(oblock_frz32bit_flag_t, dnp3_p_oblock(G_V(FROZENCTR, 32BIT_FLAG_TIME), ctr32_flag_t));
    H_RULE(oblock_frz16bit_flag_t, dnp3_p_oblock(G_V(FROZENCTR, 16BIT_FLAG_TIME), ctr16_flag_t));
    H_RULE(oblock_frz32bit_noflag, dnp3_p_oblock(G_V(FROZENCTR, 32BIT_NOFLAG), ctr32));
    H_RULE(oblock_frz16bit_noflag, dnp3_p_oblock(G_V(FROZENCTR, 16BIT_NOFLAG), ctr16));

    dnp3_p_frozenctr_rblock = dnp3_p_rblock(G(FROZENCTR),
                                            V(FROZENCTR, 32BIT_FLAG),
                                            V(FROZENCTR, 16BIT_FLAG),
                                            V(FROZENCTR, 32BIT_FLAG_TIME),
                                            V(FROZENCTR, 16BIT_FLAG_TIME),
                                            V(FROZENCTR, 32BIT_NOFLAG),
                                            V(FROZENCTR, 32BIT_NOFLAG), 0);
    dnp3_p_frozenctr_oblock = h_choice(oblock_frz32bit_flag,
                                       oblock_frz16bit_flag,
                                       oblock_frz32bit_flag_t,
                                       oblock_frz16bit_flag_t,
                                       oblock_frz32bit_noflag,
                                       oblock_frz16bit_noflag,
                                       NULL);

    // group 22: counter events...
    H_RULE(oblock_ev32bit_flag,   dnp3_p_oblock(G_V(CTREV, 32BIT_FLAG), ctr32_flag));
    H_RULE(oblock_ev16bit_flag,   dnp3_p_oblock(G_V(CTREV, 16BIT_FLAG), ctr16_flag));
    H_RULE(oblock_ev32bit_flag_t, dnp3_p_oblock(G_V(CTREV, 32BIT_FLAG_TIME), ctr32_flag_t));
    H_RULE(oblock_ev16bit_flag_t, dnp3_p_oblock(G_V(CTREV, 16BIT_FLAG_TIME), ctr16_flag_t));

    dnp3_p_ctrev_rblock = dnp3_p_rblock(G(CTREV), V(CTREV, 32BIT_FLAG),
                                                  V(CTREV, 16BIT_FLAG),
                                                  V(CTREV, 32BIT_FLAG_TIME),
                                                  V(CTREV, 16BIT_FLAG_TIME), 0);
    dnp3_p_ctrev_oblock = h_choice(oblock_ev32bit_flag,
                                   oblock_ev16bit_flag,
                                   oblock_ev32bit_flag_t,
                                   oblock_ev16bit_flag_t,
                                   NULL);

    // group 21: frozen counter events...
    H_RULE(oblock_frzev32bit_flag,   dnp3_p_oblock(G_V(FROZENCTREV, 32BIT_FLAG), ctr32_flag));
    H_RULE(oblock_frzev16bit_flag,   dnp3_p_oblock(G_V(FROZENCTREV, 16BIT_FLAG), ctr16_flag));
    H_RULE(oblock_frzev32bit_flag_t, dnp3_p_oblock(G_V(FROZENCTREV, 32BIT_FLAG_TIME), ctr32_flag_t));
    H_RULE(oblock_frzev16bit_flag_t, dnp3_p_oblock(G_V(FROZENCTREV, 16BIT_FLAG_TIME), ctr16_flag_t));

    dnp3_p_frozenctrev_rblock = dnp3_p_rblock(G(FROZENCTREV),
                                              V(FROZENCTREV, 32BIT_FLAG),
                                              V(FROZENCTREV, 16BIT_FLAG),
                                              V(FROZENCTREV, 32BIT_FLAG_TIME),
                                              V(FROZENCTREV, 16BIT_FLAG_TIME), 0);
    dnp3_p_frozenctrev_oblock = h_choice(oblock_frzev32bit_flag,
                                         oblock_frzev16bit_flag,
                                         oblock_frzev32bit_flag_t,
                                         oblock_frzev16bit_flag_t,
                                         NULL);
}


/* Delta counters are deprecated, but here's what they look like anyway.
 *
 * delta_ctr_32_flag = h_sequence(h_bits(1),                                 // ONLINE         // A10.3, variation 3
 *                                h_bits(1),                                 // RESTART
 *                                h_bits(1),                                 // COMM_LOST
 *                                h_bits(1),                                 // REMOTE_FORCED
 *                                h_bits(1),                                 // LOCAL_FORCED
 *                                h_bits(1),                                 // ROLLOVER
 *                                h_ignore(h_attr_bool(h_bits(1), is_zero)), // reserved; check that it's 0 anyway
 *                                h_ignore(h_attr_bool(h_bits(1), is_zero)), // reserved; check that it's 0 anyway
 *                                h_uint32(), NULL);
 * delta_ctr_16_flag = h_sequence(h_bits(1),                                 // ONLINE         // A10.4, variation 4
 *                                h_bits(1),                                 // RESTART
 *                                h_bits(1),                                 // COMM_LOST
 *                                h_bits(1),                                 // REMOTE_FORCED
 *                                h_bits(1),                                 // LOCAL_FORCED
 *                                h_bits(1),                                 // ROLLOVER
 *                                h_ignore(h_attr_bool(h_bits(1), is_zero)), // resreved; check that it's 0 anyway
 *                                h_ignore(h_attr_bool(h_bits(1), is_zero)), // reserved; check that it's 0 anyway
 *                                h_uint16(), NULL);
 * 
 * delta_ctr_32_no_flag = h_uint32();                                                          // A10.7, variation 7
 * delta_ctr_16_no_flag = h_uint16();                                                          // A10.8, variation 8
 *
 * frozen_delta_32_flag = h_sequence(h_bits(1),                                 // ONLINE      // A11.3, variation 3
 *                                   h_bits(1),                                 // RESTART
 *                                   h_bits(1),                                 // COMM_LOST
 *                                   h_bits(1),                                 // REMOTE_FORCED
 *                                   h_bits(1),                                 // LOCAL_FORCED
 *                                   h_bits(1),                                 // ROLLOVER
 *                                   h_ignore(h_attr_bool(h_bits(1), is_zero)), // reserved; check that it's 0 anyway
 *                                   h_ignore(h_attr_bool(h_bits(1), is_zero)), // reserved; check that it's 0 anyway
 *                                   h_uint32(), NULL);
 * frozen_delta_16_flag = h_sequence(h_bits(1),                                 // ONLINE      // A11.4, variation 4
 *                                   h_bits(1),                                 // RESTART
 *                                   h_bits(1),                                 // COMM_LOST
 *                                   h_bits(1),                                 // REMOTE_FORCED
 *                                   h_bits(1),                                 // LOCAL_FORCED
 *                                   h_bits(1),                                 // ROLLOVER
 *                                   h_ignore(h_attr_bool(h_bits(1), is_zero)), // resreved; check that it's 0 anyway
 *                                   h_ignore(h_attr_bool(h_bits(1), is_zero)), // reserved; check that it's 0 anyway
 *                                   h_uint16(), NULL);
 *
 * frozen_delta_32_flag_time = h_sequence(h_bits(1),                                 // ONLINE // A11.7, variation 7
 *                                        h_bits(1),                                 // RESTART
 *                                        h_bits(1),                                 // COMM_LOST
 *                                        h_bits(1),                                 // REMOTE_FORCED
 *                                        h_bits(1),                                 // LOCAL_FORCED
 *                                        h_bits(1),                                 // ROLLOVER
 *                                        h_ignore(h_attr_bool(h_bits(1), is_zero)), // reserved; check that it's 0 anyway
 *                                        h_ignore(h_attr_bool(h_bits(1), is_zero)), // reserved; check that it's 0 anyway 
 *                                        h_uint32(), 
 *                                        dnp3time, NULL);
 * frozen_delta_16_flag_time = h_sequence(h_bits(1),                                 // ONLINE // A11.8, variation 8
 *                                        h_bits(1),                                 // RESTART
 *                                        h_bits(1),                                 // COMM_LOST
 *                                        h_bits(1),                                 // REMOTE_FORCED
 *                                        h_bits(1),                                 // LOCAL_FORCED
 *                                        h_bits(1),                                 // ROLLOVER
 *                                        h_ignore(h_attr_bool(h_bits(1), is_zero)), // reserved; check that it's 0 anyway
 *                                        h_ignore(h_attr_bool(h_bits(1), is_zero)), // reserved; check that it's 0 anyway
 *                                        h_uint16(),  
 *                                        dnp3time, NULL);
 * 
 * frozen_delta_32_no_flag = h_uint32();                                                       // A11.11, variation 11
 * frozen_delta_16_no_flag = h_uint16();                                                       // A11.12, variation 12
 */

