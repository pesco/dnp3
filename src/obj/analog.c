#include <dnp3.h>
#include <hammer/glue.h>
#include "../hammer.h"
#include "../app.h"
#include "../util.h"

#include "analog.h"


HParser *dnp3_p_anain_rblock;
HParser *dnp3_p_anain_oblock;

HParser *dnp3_p_frozenanain_rblock;
HParser *dnp3_p_frozenanain_oblock;

HParser *dnp3_p_anainev_rblock;
HParser *dnp3_p_anainev_oblock;

HParser *dnp3_p_frozenanainev_rblock;
HParser *dnp3_p_frozenanainev_oblock;

HParser *dnp3_p_anaindeadband_rblock;
HParser *dnp3_p_anaindeadband_wblock;
HParser *dnp3_p_anaindeadband_oblock;


static HParsedToken *act_int_noflag(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    o->ana.sint = H_CAST_SINT(p->ast);
    return H_MAKE(DNP3_Object, o);
}

#define act_int32_noflag act_int_noflag
#define act_int16_noflag act_int_noflag

static HParsedToken *act_flags(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    o->flags.online          = H_FIELD_UINT(0);
    o->flags.restart         = H_FIELD_UINT(1);
    o->flags.comm_lost       = H_FIELD_UINT(2);
    o->flags.remote_forced   = H_FIELD_UINT(3);
    o->flags.local_forced    = H_FIELD_UINT(4);
    o->flags.over_range      = H_FIELD_UINT(5);
    o->flags.reference_err   = H_FIELD_UINT(6);

    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_int_flag(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (flags, value)
    o->ana.flags = H_FIELD(DNP3_Object, 0)->flags;
    o->ana.sint = H_FIELD_SINT(1);

    return H_MAKE(DNP3_Object, o);
}

#define act_int32_flag act_int_flag
#define act_int16_flag act_int_flag

static HParsedToken *act_flt_flag(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (flags, value)
    o->ana.flags = H_FIELD(DNP3_Object, 0)->flags;
    o->ana.flt = H_FIELD_FLOAT(1);

    return H_MAKE(DNP3_Object, o);
}

#define act_flt32_flag act_flt_flag
#define act_flt64_flag act_flt_flag

static HParsedToken *act_int_flag_time(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (flags, value, time)
    o->timed.ana.flags = H_FIELD(DNP3_Object, 0)->flags;
    o->timed.ana.sint = H_FIELD_SINT(1);
    o->timed.abstime = H_FIELD_UINT(2);

    return H_MAKE(DNP3_Object, o);
}

#define act_int32_flag_t act_int_flag_time
#define act_int16_flag_t act_int_flag_time

static HParsedToken *act_flt_flag_t(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (flags, value, time)
    o->timed.ana.flags = H_FIELD(DNP3_Object, 0)->flags;
    o->timed.ana.flt = H_FIELD_FLOAT(1);
    o->timed.abstime = H_FIELD_UINT(2);

    return H_MAKE(DNP3_Object, o);
}

#define act_flt32_flag_t act_flt_flag_t
#define act_flt64_flag_t act_flt_flag_t

static HParsedToken *act_deadband(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    o->ana.uint = H_CAST_UINT(p->ast);
    return H_MAKE(DNP3_Object, o);
}

#define act_deadband_32 act_deadband
#define act_deadband_16 act_deadband

static HParsedToken *act_deadband_flt(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    o->ana.flt = H_CAST_FLOAT(p->ast);
    assert(o->ana.flt >= 0);
    return H_MAKE(DNP3_Object, o);
}

static bool validate_uflt(HParseResult *p, void *user)
{
    return (H_CAST_FLOAT(p->ast) >= 0);
}

#define validate_uflt32 validate_uflt

void dnp3_p_init_analog(void)
{
    H_RULE (bit,         h_bits(1, false));
    H_RULE (reserved,    dnp3_p_reserved(1));

    H_ARULE(flags,      h_sequence(bit,    // ONLINE
                                   bit,    // RESTART
                                   bit,    // COMM_LOST
                                   bit,    // REMOTE_FORCED
                                   bit,    // LOCAL_FORCED
                                   bit,    // OVER_RANGE
                                   bit,    // REFERENCE_ERR
                                   reserved,
                                   NULL));

    H_RULE (int32,      h_int32());
    H_RULE (int16,      h_int16());
    H_RULE (flt32,      h_float32());
    H_RULE (flt64,      h_float64());
    H_VRULE(uflt32,     h_float32());   // "unsigned" float (nonnegative)

    H_ARULE(int32_noflag, int32);
    H_ARULE(int16_noflag, int16);
    H_ARULE(int32_flag,   h_sequence(flags, int32, NULL));
    H_ARULE(int16_flag,   h_sequence(flags, int16, NULL));
    H_ARULE(int32_flag_t, h_sequence(flags, int32, dnp3_p_dnp3time, NULL));
    H_ARULE(int16_flag_t, h_sequence(flags, int16, dnp3_p_dnp3time, NULL));
    H_ARULE(flt32_flag,   h_sequence(flags, flt32, NULL));
    H_ARULE(flt64_flag,   h_sequence(flags, flt64, NULL));
    H_ARULE(flt32_flag_t, h_sequence(flags, flt32, dnp3_p_dnp3time, NULL));
    H_ARULE(flt64_flag_t, h_sequence(flags, flt64, dnp3_p_dnp3time, NULL));

    H_ARULE(deadband_16,    h_uint16());
    H_ARULE(deadband_32,    h_uint32());
    H_ARULE(deadband_flt,   uflt32);

    // group 30: analog inputs...
    H_RULE(oblock_i32fl,    dnp3_p_oblock(G_V(ANAIN, 32BIT_FLAG), int32_flag));
    H_RULE(oblock_i16fl,    dnp3_p_oblock(G_V(ANAIN, 16BIT_FLAG), int16_flag));
    H_RULE(oblock_i32nofl,  dnp3_p_oblock(G_V(ANAIN, 32BIT_NOFLAG), int32_noflag));
    H_RULE(oblock_i16nofl,  dnp3_p_oblock(G_V(ANAIN, 16BIT_NOFLAG), int16_noflag));
    H_RULE(oblock_f32fl,    dnp3_p_oblock(G_V(ANAIN, FLOAT_FLAG), flt32_flag));
    H_RULE(oblock_f64fl,    dnp3_p_oblock(G_V(ANAIN, DOUBLE_FLAG), flt64_flag));

    dnp3_p_anain_rblock     = dnp3_p_rblock(G(ANAIN),
                                            V(ANAIN, 32BIT_FLAG),
                                            V(ANAIN, 16BIT_FLAG),
                                            V(ANAIN, 32BIT_NOFLAG),
                                            V(ANAIN, 16BIT_NOFLAG),
                                            V(ANAIN, FLOAT_FLAG),
                                            V(ANAIN, DOUBLE_FLAG), 0);
    dnp3_p_anain_oblock     = h_choice(oblock_i32fl, oblock_i16fl,
                                       oblock_i32nofl, oblock_i16nofl,
                                       oblock_f32fl, oblock_f64fl, NULL);

    // group 31: frozen analog inputs...
    H_RULE(oblock_frzi32fl,    dnp3_p_oblock(G_V(FROZENANAIN, 32BIT_FLAG), int32_flag));
    H_RULE(oblock_frzi16fl,    dnp3_p_oblock(G_V(FROZENANAIN, 16BIT_FLAG), int16_flag));
    H_RULE(oblock_frzi32fl_t,  dnp3_p_oblock(G_V(FROZENANAIN, 32BIT_FLAG_TIME), int32_flag_t));
    H_RULE(oblock_frzi16fl_t,  dnp3_p_oblock(G_V(FROZENANAIN, 16BIT_FLAG_TIME), int16_flag_t));
    H_RULE(oblock_frzi32nofl,  dnp3_p_oblock(G_V(FROZENANAIN, 32BIT_NOFLAG), int32_noflag));
    H_RULE(oblock_frzi16nofl,  dnp3_p_oblock(G_V(FROZENANAIN, 16BIT_NOFLAG), int16_noflag));
    H_RULE(oblock_frzf32fl,    dnp3_p_oblock(G_V(FROZENANAIN, FLOAT_FLAG), flt32_flag));
    H_RULE(oblock_frzf64fl,    dnp3_p_oblock(G_V(FROZENANAIN, DOUBLE_FLAG), flt64_flag));

    dnp3_p_frozenanain_rblock     = dnp3_p_rblock(G(FROZENANAIN),
                                                  V(FROZENANAIN, 32BIT_FLAG),
                                                  V(FROZENANAIN, 16BIT_FLAG),
                                                  V(FROZENANAIN, 32BIT_FLAG_TIME),
                                                  V(FROZENANAIN, 16BIT_FLAG_TIME),
                                                  V(FROZENANAIN, 32BIT_NOFLAG),
                                                  V(FROZENANAIN, 16BIT_NOFLAG),
                                                  V(FROZENANAIN, FLOAT_FLAG),
                                                  V(FROZENANAIN, DOUBLE_FLAG), 0);
    dnp3_p_frozenanain_oblock     = h_choice(oblock_frzi32fl, oblock_frzi16fl,
                                             oblock_frzi32fl_t, oblock_frzi16fl_t,
                                             oblock_frzi32nofl, oblock_frzi16nofl,
                                             oblock_frzf32fl, oblock_frzf64fl, NULL);

    // group 32: analog input events...
    H_RULE(oblock_evi32fl,    dnp3_p_oblock(G_V(ANAINEV, 32BIT_FLAG), int32_flag));
    H_RULE(oblock_evi16fl,    dnp3_p_oblock(G_V(ANAINEV, 16BIT_FLAG), int16_flag));
    H_RULE(oblock_evi32fl_t,  dnp3_p_oblock(G_V(ANAINEV, 32BIT_FLAG_TIME), int32_flag_t));
    H_RULE(oblock_evi16fl_t,  dnp3_p_oblock(G_V(ANAINEV, 16BIT_FLAG_TIME), int16_flag_t));
    H_RULE(oblock_evf32fl,    dnp3_p_oblock(G_V(ANAINEV, FLOAT_FLAG), flt32_flag));
    H_RULE(oblock_evf64fl,    dnp3_p_oblock(G_V(ANAINEV, DOUBLE_FLAG), flt64_flag));
    H_RULE(oblock_evf32fl_t,  dnp3_p_oblock(G_V(ANAINEV, FLOAT_FLAG_TIME), flt32_flag_t));
    H_RULE(oblock_evf64fl_t,  dnp3_p_oblock(G_V(ANAINEV, DOUBLE_FLAG_TIME), flt64_flag_t));

    dnp3_p_anainev_rblock     = dnp3_p_rblock(G(ANAINEV),
                                              V(ANAINEV, 32BIT_FLAG),
                                              V(ANAINEV, 16BIT_FLAG),
                                              V(ANAINEV, 32BIT_FLAG_TIME),
                                              V(ANAINEV, 16BIT_FLAG_TIME),
                                              V(ANAINEV, FLOAT_FLAG),
                                              V(ANAINEV, DOUBLE_FLAG),
                                              V(ANAINEV, FLOAT_FLAG_TIME),
                                              V(ANAINEV, DOUBLE_FLAG_TIME), 0);
    dnp3_p_anainev_oblock     = h_choice(oblock_evi32fl, oblock_evi16fl,
                                         oblock_evi32fl_t, oblock_evi16fl_t,
                                         oblock_evf32fl, oblock_evf64fl,
                                         oblock_evf32fl_t, oblock_evf64fl_t, NULL);

    // group 33: frozen analog input events...
    H_RULE(oblock_frzevi32fl,    dnp3_p_oblock(G_V(FROZENANAINEV, 32BIT_FLAG), int32_flag));
    H_RULE(oblock_frzevi16fl,    dnp3_p_oblock(G_V(FROZENANAINEV, 16BIT_FLAG), int16_flag));
    H_RULE(oblock_frzevi32fl_t,  dnp3_p_oblock(G_V(FROZENANAINEV, 32BIT_FLAG_TIME), int32_flag_t));
    H_RULE(oblock_frzevi16fl_t,  dnp3_p_oblock(G_V(FROZENANAINEV, 16BIT_FLAG_TIME), int16_flag_t));
    H_RULE(oblock_frzevf32fl,    dnp3_p_oblock(G_V(FROZENANAINEV, FLOAT_FLAG), flt32_flag));
    H_RULE(oblock_frzevf64fl,    dnp3_p_oblock(G_V(FROZENANAINEV, DOUBLE_FLAG), flt64_flag));
    H_RULE(oblock_frzevf32fl_t,  dnp3_p_oblock(G_V(FROZENANAINEV, FLOAT_FLAG_TIME), flt32_flag_t));
    H_RULE(oblock_frzevf64fl_t,  dnp3_p_oblock(G_V(FROZENANAINEV, DOUBLE_FLAG_TIME), flt64_flag_t));

    dnp3_p_frozenanainev_rblock     = dnp3_p_rblock(G(FROZENANAINEV),
                                                    V(FROZENANAINEV, 32BIT_FLAG),
                                                    V(FROZENANAINEV, 16BIT_FLAG),
                                                    V(FROZENANAINEV, 32BIT_FLAG_TIME),
                                                    V(FROZENANAINEV, 16BIT_FLAG_TIME),
                                                    V(FROZENANAINEV, FLOAT_FLAG),
                                                    V(FROZENANAINEV, DOUBLE_FLAG),
                                                    V(FROZENANAINEV, FLOAT_FLAG_TIME),
                                                    V(FROZENANAINEV, DOUBLE_FLAG_TIME), 0);
    dnp3_p_frozenanainev_oblock     = h_choice(oblock_frzevi32fl, oblock_frzevi16fl,
                                               oblock_frzevi32fl_t, oblock_frzevi16fl_t,
                                               oblock_frzevf32fl, oblock_frzevf64fl,
                                               oblock_frzevf32fl_t, oblock_frzevf64fl_t, NULL);

    // group 34: analog input deadbands...
    H_RULE(oblock_dbi16,    dnp3_p_oblock(G_V(ANAINDEADBAND, 16BIT), deadband_16));
    H_RULE(oblock_dbi32,    dnp3_p_oblock(G_V(ANAINDEADBAND, 32BIT), deadband_32));
    H_RULE(oblock_dbf32,    dnp3_p_oblock(G_V(ANAINDEADBAND, FLOAT), deadband_flt));

    dnp3_p_anaindeadband_rblock = dnp3_p_rblock(G(ANAINDEADBAND),
                                                V(ANAINDEADBAND, 16BIT),
                                                V(ANAINDEADBAND, 32BIT),
                                                V(ANAINDEADBAND, FLOAT), 0);
    dnp3_p_anaindeadband_wblock = h_choice(oblock_dbi16, oblock_dbi32, oblock_dbf32, NULL);
    dnp3_p_anaindeadband_oblock = h_choice(oblock_dbi16, oblock_dbi32, oblock_dbf32, NULL);
                                      
}
