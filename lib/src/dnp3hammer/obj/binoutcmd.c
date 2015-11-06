#include <dnp3.h>
#include <hammer/glue.h>
#include "../app.h"
#include "../util.h"

#include "binoutcmd.h"


HParser *dnp3_p_binoutcmdev_rblock;
HParser *dnp3_p_binoutcmdev_oblock;

HParser *dnp3_p_binoutcmd_rblock;

HParser *dnp3_p_g12v1_binoutcmd_crob_oblock;
HParser *dnp3_p_g12v2_binoutcmd_pcb_oblock;
HParser *dnp3_p_g12v3_binoutcmd_pcm_oblock;
HParser *dnp3_p_g12v3_binoutcmd_pcm_rblock;


static HParsedToken *act_notime(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (cs, status)
    o->cmdev.status = H_FIELD_UINT(0);
    o->cmdev.cs     = H_FIELD_UINT(1);

    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_abstime(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (cs, status, abstime)
    o->timed.cmdev.status = H_FIELD_UINT(0);
    o->timed.cmdev.cs     = H_FIELD_UINT(1);
    o->timed.abstime      = H_FIELD_UINT(2);

    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_crob(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);

    // p = (optype,qu,cr,tcc,count,on,off,status)
    o->cmd.optype  = H_FIELD_UINT(0);
    o->cmd.queue   = H_FIELD_UINT(1);
    o->cmd.clear   = H_FIELD_UINT(2);
    o->cmd.tcc     = H_FIELD_UINT(3);
    o->cmd.count   = H_FIELD_UINT(4);
    o->cmd.on      = H_FIELD_UINT(5);
    o->cmd.off     = H_FIELD_UINT(6);
    o->cmd.status  = H_FIELD_UINT(7);

    return H_MAKE(DNP3_Object, o);
}

static HParsedToken *act_packed(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    o->bit = H_CAST_UINT(p->ast);
    return H_MAKE(DNP3_Object, o);
}

void dnp3_p_init_binoutcmd(void)
{
    H_RULE (bit,        h_bits(1, false));

    H_RULE (cs,         bit);
    H_RULE (status,     h_bits(7, false));

    H_ARULE(notime,  h_sequence(status, cs, NULL));
    H_ARULE(abstime, h_sequence(status, cs, dnp3_p_dnp3time, NULL));

    H_RULE (tcc,    h_int_range(h_bits(2, false), 0, 2));
    H_ARULE(crob,   h_sequence(h_bits(4, false),    // op type
                               bit,                 // queue flag (obsolete)
                               bit,                 // clear flag
                               tcc,
                               h_uint8(),           // count
                               h_uint32(),          // on-time [ms]
                               h_uint32(),          // off-time [ms]
                               status,              // 7 bits
                               dnp3_p_reserved(1),
                               NULL));
    H_ARULE(packed, bit);

    // group 12 (binary output commands)...
    dnp3_p_g12v1_binoutcmd_crob_oblock = dnp3_p_oblock(G_V(BINOUTCMD, CROB), crob);
    dnp3_p_g12v2_binoutcmd_pcb_oblock  = dnp3_p_single(G_V(BINOUTCMD, PCB), crob);
    dnp3_p_g12v3_binoutcmd_pcm_oblock  = dnp3_p_oblock_packed(G_V(BINOUTCMD, PCM), packed);
    dnp3_p_g12v3_binoutcmd_pcm_rblock  = dnp3_p_specific_rblock(G_V(BINOUTCMD, PCM));

    dnp3_p_binoutcmd_rblock = dnp3_p_rblock(G(BINOUTCMD),
                                            V(BINOUTCMD, CROB),
                                            V(BINOUTCMD, PCB),
                                            V(BINOUTCMD, PCM), 0);

    // group 13 (binary output command events)...
    H_RULE(oblock_notime,   dnp3_p_oblock(G_V(BINOUTCMDEV, NOTIME),  notime));
    H_RULE(oblock_abstime,  dnp3_p_oblock(G_V(BINOUTCMDEV, ABSTIME), abstime));

    dnp3_p_binoutcmdev_rblock = dnp3_p_rblock(G(BINOUTCMDEV),
                                              V(BINOUTCMDEV, NOTIME),
                                              V(BINOUTCMDEV, ABSTIME), 0);
    dnp3_p_binoutcmdev_oblock = h_choice(oblock_notime,
                                         oblock_abstime, NULL);
}
