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

HParser *dnp3_p_anaoutstatus_rblock;
HParser *dnp3_p_anaoutstatus_oblock;

HParser *dnp3_p_anaout_sblock;
HParser *dnp3_p_anaout_oblock;

HParser *dnp3_p_anaoutev_rblock;
HParser *dnp3_p_anaoutev_oblock;

HParser *dnp3_p_anaoutcmdev_rblock;
HParser *dnp3_p_anaoutcmdev_oblock;


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

static HParsedToken *act_int_out(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (value, status)
    o->ana.sint = H_FIELD_SINT(0);
    o->ana.status = H_FIELD_UINT(1);

    return H_MAKE(DNP3_Object, o);
}

#define act_int32_out act_int_out
#define act_int16_out act_int_out
#define act_int32_out_s act_int_out
#define act_int16_out_s act_int_out

static HParsedToken *act_flt_out(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (value, status)
    o->ana.flt = H_FIELD_FLOAT(0);
    o->ana.status = H_FIELD_UINT(1);

    return H_MAKE(DNP3_Object, o);
}

#define act_flt32_out act_flt_out
#define act_flt64_out act_flt_out
#define act_flt32_out_s act_flt_out
#define act_flt64_out_s act_flt_out

static HParsedToken *act_int_cmdev(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (status, value, time?)
    o->ana.status = H_FIELD_UINT(0);
    o->ana.sint = H_FIELD_SINT(1);

    return H_MAKE(DNP3_Object, o);
}

#define act_int32_cmdev act_int_cmdev
#define act_int16_cmdev act_int_cmdev

static HParsedToken *act_int_cmdev_time(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (status, value, time?)
    o->timed.ana.status = H_FIELD_UINT(0);
    o->timed.ana.sint = H_FIELD_SINT(1);
    o->timed.abstime = H_FIELD_UINT(2);

    return H_MAKE(DNP3_Object, o);
}

#define act_int32_cmdev_t act_int_cmdev_time
#define act_int16_cmdev_t act_int_cmdev_time

static HParsedToken *act_flt_cmdev(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (status, value, time?)
    o->ana.status = H_FIELD_UINT(0);
    o->ana.flt = H_FIELD_FLOAT(1);

    return H_MAKE(DNP3_Object, o);
}

#define act_flt32_cmdev act_flt_cmdev
#define act_flt64_cmdev act_flt_cmdev

static HParsedToken *act_flt_cmdev_time(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (status, value, time?)
    o->timed.ana.status = H_FIELD_UINT(0);
    o->timed.ana.flt = H_FIELD_FLOAT(1);
    o->timed.abstime = H_FIELD_UINT(2);

    return H_MAKE(DNP3_Object, o);
}

#define act_flt32_cmdev_t act_flt_cmdev_time
#define act_flt64_cmdev_t act_flt_cmdev_time

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

    H_RULE (status,         h_left(h_bits(7, false), reserved));
    H_RULE (status8,        h_uint8());
        // XXX should status always be 7 bits?
        // for g41 IEEE 1815-2012 says range 0-255 but for g43 it says 7.
    H_RULE (zero,           h_ch(0));

    H_ARULE(int32_out,      h_sequence(int32, status8, NULL));
    H_ARULE(int16_out,      h_sequence(int16, status8, NULL));
    H_ARULE(flt32_out,      h_sequence(flt32, status8, NULL));
    H_ARULE(flt64_out,      h_sequence(flt64, status8, NULL));
    H_ARULE(int32_out_s,    h_sequence(int32, zero, NULL));
    H_ARULE(int16_out_s,    h_sequence(int16, zero, NULL));
    H_ARULE(flt32_out_s,    h_sequence(flt32, zero, NULL));
    H_ARULE(flt64_out_s,    h_sequence(flt64, zero, NULL));

    H_ARULE(int32_cmdev,    h_sequence(status, int32, NULL));
    H_ARULE(int16_cmdev,    h_sequence(status, int16, NULL));
    H_ARULE(flt32_cmdev,    h_sequence(status, flt32, NULL));
    H_ARULE(flt64_cmdev,    h_sequence(status, flt64, NULL));
    H_ARULE(int32_cmdev_t,  h_sequence(status, int32, dnp3_p_dnp3time, NULL));
    H_ARULE(int16_cmdev_t,  h_sequence(status, int16, dnp3_p_dnp3time, NULL));
    H_ARULE(flt32_cmdev_t,  h_sequence(status, flt32, dnp3_p_dnp3time, NULL));
    H_ARULE(flt64_cmdev_t,  h_sequence(status, flt64, dnp3_p_dnp3time, NULL));

    // group 30: analog inputs...
    H_RULE(oblock_i32fl,    dnp3_p_oblock(G_V(ANAIN, 32BIT), int32_flag));
    H_RULE(oblock_i16fl,    dnp3_p_oblock(G_V(ANAIN, 16BIT), int16_flag));
    H_RULE(oblock_i32nofl,  dnp3_p_oblock(G_V(ANAIN, 32BIT_NOFLAG), int32_noflag));
    H_RULE(oblock_i16nofl,  dnp3_p_oblock(G_V(ANAIN, 16BIT_NOFLAG), int16_noflag));
    H_RULE(oblock_f32fl,    dnp3_p_oblock(G_V(ANAIN, FLOAT), flt32_flag));
    H_RULE(oblock_f64fl,    dnp3_p_oblock(G_V(ANAIN, DOUBLE), flt64_flag));

    dnp3_p_anain_rblock     = dnp3_p_rblock(G(ANAIN),
                                            V(ANAIN, 32BIT),
                                            V(ANAIN, 16BIT),
                                            V(ANAIN, 32BIT_NOFLAG),
                                            V(ANAIN, 16BIT_NOFLAG),
                                            V(ANAIN, FLOAT),
                                            V(ANAIN, DOUBLE), 0);
    dnp3_p_anain_oblock     = h_choice(oblock_i32fl, oblock_i16fl,
                                       oblock_i32nofl, oblock_i16nofl,
                                       oblock_f32fl, oblock_f64fl, NULL);

    // group 31: frozen analog inputs...
    H_RULE(oblock_frzi32fl,    dnp3_p_oblock(G_V(FROZENANAIN, 32BIT), int32_flag));
    H_RULE(oblock_frzi16fl,    dnp3_p_oblock(G_V(FROZENANAIN, 16BIT), int16_flag));
    H_RULE(oblock_frzi32fl_t,  dnp3_p_oblock(G_V(FROZENANAIN, 32BIT_TIME), int32_flag_t));
    H_RULE(oblock_frzi16fl_t,  dnp3_p_oblock(G_V(FROZENANAIN, 16BIT_TIME), int16_flag_t));
    H_RULE(oblock_frzi32nofl,  dnp3_p_oblock(G_V(FROZENANAIN, 32BIT_NOFLAG), int32_noflag));
    H_RULE(oblock_frzi16nofl,  dnp3_p_oblock(G_V(FROZENANAIN, 16BIT_NOFLAG), int16_noflag));
    H_RULE(oblock_frzf32fl,    dnp3_p_oblock(G_V(FROZENANAIN, FLOAT), flt32_flag));
    H_RULE(oblock_frzf64fl,    dnp3_p_oblock(G_V(FROZENANAIN, DOUBLE), flt64_flag));

    dnp3_p_frozenanain_rblock     = dnp3_p_rblock(G(FROZENANAIN),
                                                  V(FROZENANAIN, 32BIT),
                                                  V(FROZENANAIN, 16BIT),
                                                  V(FROZENANAIN, 32BIT_TIME),
                                                  V(FROZENANAIN, 16BIT_TIME),
                                                  V(FROZENANAIN, 32BIT_NOFLAG),
                                                  V(FROZENANAIN, 16BIT_NOFLAG),
                                                  V(FROZENANAIN, FLOAT),
                                                  V(FROZENANAIN, DOUBLE), 0);
    dnp3_p_frozenanain_oblock     = h_choice(oblock_frzi32fl, oblock_frzi16fl,
                                             oblock_frzi32fl_t, oblock_frzi16fl_t,
                                             oblock_frzi32nofl, oblock_frzi16nofl,
                                             oblock_frzf32fl, oblock_frzf64fl, NULL);

    // group 32: analog input events...
    H_RULE(oblock_evi32fl,    dnp3_p_oblock(G_V(ANAINEV, 32BIT), int32_flag));
    H_RULE(oblock_evi16fl,    dnp3_p_oblock(G_V(ANAINEV, 16BIT), int16_flag));
    H_RULE(oblock_evi32fl_t,  dnp3_p_oblock(G_V(ANAINEV, 32BIT_TIME), int32_flag_t));
    H_RULE(oblock_evi16fl_t,  dnp3_p_oblock(G_V(ANAINEV, 16BIT_TIME), int16_flag_t));
    H_RULE(oblock_evf32fl,    dnp3_p_oblock(G_V(ANAINEV, FLOAT), flt32_flag));
    H_RULE(oblock_evf64fl,    dnp3_p_oblock(G_V(ANAINEV, DOUBLE), flt64_flag));
    H_RULE(oblock_evf32fl_t,  dnp3_p_oblock(G_V(ANAINEV, FLOAT_TIME), flt32_flag_t));
    H_RULE(oblock_evf64fl_t,  dnp3_p_oblock(G_V(ANAINEV, DOUBLE_TIME), flt64_flag_t));

    dnp3_p_anainev_rblock     = dnp3_p_rblock(G(ANAINEV),
                                              V(ANAINEV, 32BIT),
                                              V(ANAINEV, 16BIT),
                                              V(ANAINEV, 32BIT_TIME),
                                              V(ANAINEV, 16BIT_TIME),
                                              V(ANAINEV, FLOAT),
                                              V(ANAINEV, DOUBLE),
                                              V(ANAINEV, FLOAT_TIME),
                                              V(ANAINEV, DOUBLE_TIME), 0);
    dnp3_p_anainev_oblock     = h_choice(oblock_evi32fl, oblock_evi16fl,
                                         oblock_evi32fl_t, oblock_evi16fl_t,
                                         oblock_evf32fl, oblock_evf64fl,
                                         oblock_evf32fl_t, oblock_evf64fl_t, NULL);

    // group 33: frozen analog input events...
    H_RULE(oblock_frzevi32fl,    dnp3_p_oblock(G_V(FROZENANAINEV, 32BIT), int32_flag));
    H_RULE(oblock_frzevi16fl,    dnp3_p_oblock(G_V(FROZENANAINEV, 16BIT), int16_flag));
    H_RULE(oblock_frzevi32fl_t,  dnp3_p_oblock(G_V(FROZENANAINEV, 32BIT_TIME), int32_flag_t));
    H_RULE(oblock_frzevi16fl_t,  dnp3_p_oblock(G_V(FROZENANAINEV, 16BIT_TIME), int16_flag_t));
    H_RULE(oblock_frzevf32fl,    dnp3_p_oblock(G_V(FROZENANAINEV, FLOAT), flt32_flag));
    H_RULE(oblock_frzevf64fl,    dnp3_p_oblock(G_V(FROZENANAINEV, DOUBLE), flt64_flag));
    H_RULE(oblock_frzevf32fl_t,  dnp3_p_oblock(G_V(FROZENANAINEV, FLOAT_TIME), flt32_flag_t));
    H_RULE(oblock_frzevf64fl_t,  dnp3_p_oblock(G_V(FROZENANAINEV, DOUBLE_TIME), flt64_flag_t));

    dnp3_p_frozenanainev_rblock     = dnp3_p_rblock(G(FROZENANAINEV),
                                                    V(FROZENANAINEV, 32BIT),
                                                    V(FROZENANAINEV, 16BIT),
                                                    V(FROZENANAINEV, 32BIT_TIME),
                                                    V(FROZENANAINEV, 16BIT_TIME),
                                                    V(FROZENANAINEV, FLOAT),
                                                    V(FROZENANAINEV, DOUBLE),
                                                    V(FROZENANAINEV, FLOAT_TIME),
                                                    V(FROZENANAINEV, DOUBLE_TIME), 0);
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

    // group 40: analog output status...
    H_RULE(oblock_stati32,    dnp3_p_oblock(G_V(ANAOUTSTATUS, 32BIT), int32_flag));
    H_RULE(oblock_stati16,    dnp3_p_oblock(G_V(ANAOUTSTATUS, 16BIT), int16_flag));
    H_RULE(oblock_statf32,    dnp3_p_oblock(G_V(ANAOUTSTATUS, FLOAT), flt32_flag));
    H_RULE(oblock_statf64,    dnp3_p_oblock(G_V(ANAOUTSTATUS, DOUBLE), flt64_flag));

    dnp3_p_anaoutstatus_rblock = dnp3_p_rblock(G(ANAOUTSTATUS),
                                               V(ANAOUTSTATUS, 32BIT),
                                               V(ANAOUTSTATUS, 16BIT),
                                               V(ANAOUTSTATUS, FLOAT),
                                               V(ANAOUTSTATUS, DOUBLE), 0);
    dnp3_p_anaoutstatus_oblock = h_choice(oblock_stati32, oblock_stati16,
                                          oblock_statf32, oblock_statf64, NULL);

    // group 41: analog outputs...
    H_RULE(oblock_outi32_s,  dnp3_p_oblock(G_V(ANAOUT, 32BIT), int32_out_s));
    H_RULE(oblock_outi16_s,  dnp3_p_oblock(G_V(ANAOUT, 16BIT), int16_out_s));
    H_RULE(oblock_outf32_s,  dnp3_p_oblock(G_V(ANAOUT, FLOAT), flt32_out_s));
    H_RULE(oblock_outf64_s,  dnp3_p_oblock(G_V(ANAOUT, DOUBLE), flt64_out_s));

    H_RULE(oblock_outi32,    dnp3_p_oblock(G_V(ANAOUT, 32BIT), int32_out));
    H_RULE(oblock_outi16,    dnp3_p_oblock(G_V(ANAOUT, 16BIT), int16_out));
    H_RULE(oblock_outf32,    dnp3_p_oblock(G_V(ANAOUT, FLOAT), flt32_out));
    H_RULE(oblock_outf64,    dnp3_p_oblock(G_V(ANAOUT, DOUBLE), flt64_out));

    // the 's' is for "select" (the first function code this is relevant for).
    // these are the variants where the "control status" field must be zero.
    dnp3_p_anaout_sblock     = h_choice(oblock_outi32_s, oblock_outi16_s,
                                        oblock_outf32_s, oblock_outf64_s, NULL);
    dnp3_p_anaout_oblock     = h_choice(oblock_outi32, oblock_outi16,
                                        oblock_outf32, oblock_outf64, NULL);

    // group 42: analog output events...
    H_RULE(oblock_outevi32,    dnp3_p_oblock(G_V(ANAOUTEV, 32BIT), int32_flag));
    H_RULE(oblock_outevi16,    dnp3_p_oblock(G_V(ANAOUTEV, 16BIT), int16_flag));
    H_RULE(oblock_outevi32_t,  dnp3_p_oblock(G_V(ANAOUTEV, 32BIT_TIME), int32_flag_t));
    H_RULE(oblock_outevi16_t,  dnp3_p_oblock(G_V(ANAOUTEV, 16BIT_TIME), int16_flag_t));
    H_RULE(oblock_outevf32,    dnp3_p_oblock(G_V(ANAOUTEV, FLOAT), flt32_flag));
    H_RULE(oblock_outevf64,    dnp3_p_oblock(G_V(ANAOUTEV, DOUBLE), flt64_flag));
    H_RULE(oblock_outevf32_t,  dnp3_p_oblock(G_V(ANAOUTEV, FLOAT_TIME), flt32_flag_t));
    H_RULE(oblock_outevf64_t,  dnp3_p_oblock(G_V(ANAOUTEV, DOUBLE_TIME), flt64_flag_t));

    dnp3_p_anaoutev_rblock = dnp3_p_rblock(G(ANAOUTEV),
                                           V(ANAOUTEV, 32BIT),
                                           V(ANAOUTEV, 16BIT),
                                           V(ANAOUTEV, 32BIT_TIME),
                                           V(ANAOUTEV, 16BIT_TIME),
                                           V(ANAOUTEV, FLOAT),
                                           V(ANAOUTEV, DOUBLE),
                                           V(ANAOUTEV, FLOAT_TIME),
                                           V(ANAOUTEV, DOUBLE_TIME), 0);
    dnp3_p_anaoutev_oblock = h_choice(oblock_outevi32, oblock_outevi16,
                                      oblock_outevi32_t, oblock_outevi16_t,
                                      oblock_outevf32, oblock_outevf64,
                                      oblock_outevf32_t, oblock_outevf64_t, NULL);

    // group 43: analog output command events...
    H_RULE(oblock_cmdevi32,    dnp3_p_oblock(G_V(ANAOUTCMDEV, 32BIT), int32_cmdev));
    H_RULE(oblock_cmdevi16,    dnp3_p_oblock(G_V(ANAOUTCMDEV, 16BIT), int16_cmdev));
    H_RULE(oblock_cmdevi32_t,  dnp3_p_oblock(G_V(ANAOUTCMDEV, 32BIT_TIME), int32_cmdev_t));
    H_RULE(oblock_cmdevi16_t,  dnp3_p_oblock(G_V(ANAOUTCMDEV, 16BIT_TIME), int16_cmdev_t));
    H_RULE(oblock_cmdevf32,    dnp3_p_oblock(G_V(ANAOUTCMDEV, FLOAT), flt32_cmdev));
    H_RULE(oblock_cmdevf64,    dnp3_p_oblock(G_V(ANAOUTCMDEV, DOUBLE), flt64_cmdev));
    H_RULE(oblock_cmdevf32_t,  dnp3_p_oblock(G_V(ANAOUTCMDEV, FLOAT_TIME), flt32_cmdev_t));
    H_RULE(oblock_cmdevf64_t,  dnp3_p_oblock(G_V(ANAOUTCMDEV, DOUBLE_TIME), flt64_cmdev_t));

    dnp3_p_anaoutcmdev_rblock = dnp3_p_rblock(G(ANAOUTCMDEV),
                                              V(ANAOUTCMDEV, 32BIT),
                                              V(ANAOUTCMDEV, 16BIT),
                                              V(ANAOUTCMDEV, 32BIT_TIME),
                                              V(ANAOUTCMDEV, 16BIT_TIME),
                                              V(ANAOUTCMDEV, FLOAT),
                                              V(ANAOUTCMDEV, DOUBLE),
                                              V(ANAOUTCMDEV, FLOAT_TIME),
                                              V(ANAOUTCMDEV, DOUBLE_TIME), 0);
    dnp3_p_anaoutcmdev_oblock = h_choice(oblock_cmdevi32, oblock_cmdevi16,
                                         oblock_cmdevi32_t, oblock_cmdevi16_t,
                                         oblock_cmdevf32, oblock_cmdevf64,
                                         oblock_cmdevf32_t, oblock_cmdevf64_t, NULL);
}
