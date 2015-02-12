#include <dnp3.h>
#include <hammer/glue.h>
#include "app.h"
#include "binary.h"
#include "time.h"

#include "g2_bininev.h"


HParser *dnp3_p_bininev_rblock;
HParser *dnp3_p_bininev_oblock;

#define act_abs dnp3_p_act_abstime
#define act_rel dnp3_p_act_reltime

void dnp3_p_init_g2_bininev(void)
{
    H_ARULE(abs, h_sequence(dnp3_p_bin_flags, dnp3_p_abstime, NULL));
    H_ARULE(rel, h_sequence(dnp3_p_bin_flags, dnp3_p_reltime, NULL));

    H_RULE(oblock_notime,   dnp3_p_oblock(G(BININEV), V(NOTIME), dnp3_p_bin_flags));
    H_RULE(oblock_abstime,  dnp3_p_oblock(G(BININEV), V(ABSTIME), abs));
    H_RULE(oblock_reltime,  dnp3_p_oblock(G(BININEV), V(RELTIME), rel));

    dnp3_p_bininev_rblock = dnp3_p_rblock(G(BININEV),
                                          V(NOTIME), V(ABSTIME), V(RELTIME), 0);
    dnp3_p_bininev_oblock = h_choice(oblock_notime,
                                     oblock_abstime,
                                     oblock_reltime, NULL);
}
