// XXX move g13 into obj/command.c or so

#include <dnp3.h>
#include <hammer/glue.h>
#include "app.h"
#include "util.h"

#include "g13_binoutcmdev.h"


HParser *dnp3_p_binoutcmdev_rblock;
HParser *dnp3_p_binoutcmdev_oblock;

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

void dnp3_p_init_g13_binoutcmdev(void)
{
    H_RULE (cs,         h_bits(1, false));
    H_RULE (status,     h_bits(7, false));

    H_ARULE(notime,  h_sequence(status, cs, NULL));
    H_ARULE(abstime, h_sequence(status, cs, dnp3_p_dnp3time, NULL));

    H_RULE(oblock_notime,   dnp3_p_oblock(G(BINOUTCMDEV), V(NOTIME),  notime));
    H_RULE(oblock_abstime,  dnp3_p_oblock(G(BINOUTCMDEV), V(ABSTIME), abstime));

    dnp3_p_binoutcmdev_rblock = dnp3_p_rblock(G(BINOUTCMDEV),
                                              V(NOTIME), V(ABSTIME), 0);
    dnp3_p_binoutcmdev_oblock = h_choice(oblock_notime,
                                         oblock_abstime, NULL);
}
