#include <dnp3hammer.h>

#include <hammer/glue.h>
#include "../app.h"
#include "../util.h"

#include "time.h"


HParser *dnp3_p_g50v1_time_rblock;
HParser *dnp3_p_g50v4_indexed_time_rblock;

HParser *dnp3_p_g50v1_time_oblock;
HParser *dnp3_p_g50v2_time_interval_oblock;
HParser *dnp3_p_g50v3_recorded_time_oblock;
HParser *dnp3_p_g50v4_indexed_time_oblock;

HParser *dnp3_p_cto_oblock;

HParser *dnp3_p_delay_oblock;


static HParsedToken *act_time(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    
    o->time.abstime = H_CAST_UINT(p->ast);

    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_time_interval(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    
    // p = (time, interval)
    o->time.abstime  = H_FIELD_UINT(0);
    o->time.interval = H_FIELD_UINT(1);

    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_indexed_time(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    
    // p = (time, interval, unit)
    o->time.abstime  = H_FIELD_UINT(0);
    o->time.interval = H_FIELD_UINT(1);
    o->time.unit     = H_FIELD_UINT(2);

    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_delay_s(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // convert to milliseconds
    o->delay = H_CAST_UINT(p->ast) * 1000;

    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_delay_ms(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    o->delay = H_CAST_UINT(p->ast);

    return H_MAKE(DNP3_Object, o);
}

void dnp3_p_init_time(void)
{
    H_RULE (abstime,        dnp3_p_dnp3time);
    H_RULE (interval,       h_uint32());        // [ms]
    H_RULE (unit,           h_uint8());         // DNP3_IntervalUnit

    H_ARULE(time,           abstime);
    H_ARULE(time_interval,  h_sequence(abstime, interval, NULL));
    H_ARULE(indexed_time,   h_sequence(abstime, interval, unit, NULL));

    H_ARULE(delay_s,        h_uint16());
    H_ARULE(delay_ms,       h_uint16());


    // group 50 (times)...
    dnp3_p_g50v1_time_rblock = dnp3_p_single_rblock(G_V(TIME, TIME));
    dnp3_p_g50v1_time_oblock = dnp3_p_single(G_V(TIME, TIME), time);

    dnp3_p_g50v2_time_interval_oblock
            = dnp3_p_single(G_V(TIME, TIME_INTERVAL), time_interval);

    dnp3_p_g50v3_recorded_time_oblock
            = dnp3_p_single(G_V(TIME, RECORDED_TIME), time);

    dnp3_p_g50v4_indexed_time_rblock
            = dnp3_p_specific_rblock(G(TIME), V(TIME, INDEXED_TIME));
    dnp3_p_g50v4_indexed_time_oblock
            = dnp3_p_oblock(G_V(TIME, INDEXED_TIME), indexed_time);

    // group 51 (common time-of-occurances)...
    // XXX is single (qc=07,range=1) correct for group 51 (CTO)?
    H_RULE (oblock_cto_sync,    dnp3_p_single(G_V(CTO, SYNC), time));
    H_RULE (oblock_cto_unsync,  dnp3_p_single(G_V(CTO, UNSYNC), time));

    dnp3_p_cto_oblock = h_choice(oblock_cto_sync, oblock_cto_unsync, NULL);

    // group 52 (delays)...
    // XXX is single (qc=07,range=1) correct for group 52 (delays)?
    H_RULE (oblock_delay_s,     dnp3_p_single(G_V(DELAY, S), delay_s));
    H_RULE (oblock_delay_ms,    dnp3_p_single(G_V(DELAY, MS), delay_ms));

    dnp3_p_delay_oblock = h_choice(oblock_delay_s, oblock_delay_ms, NULL);
}
