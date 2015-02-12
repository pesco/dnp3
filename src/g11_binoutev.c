#include <dnp3.h>
#include <hammer/glue.h>
#include "app.h"
#include "binary.h"

#include "g11_binoutev.h"


HParser *dnp3_p_binoutev_rblock;
HParser *dnp3_p_binoutev_oblock;

#define act_abs dnp3_p_act_abstime

void dnp3_p_init_g10_binoutev(void)
{
    H_ARULE(abs, h_sequence(dnp3_p_bin_flags, dnp3_p_abstime, NULL));

    H_RULE(oblock_notime,   dnp3_p_oblock(G(BINOUTEV), V(NOTIME), dnp3_p_bin_flags));
    H_RULE(oblock_abstime,  dnp3_p_oblock(G(BINOUTEV), V(ABSTIME), abs));

    dnp3_p_binoutev_rblock = dnp3_p_rblock(G(BINOUTEV),
                                           V(NOTIME), V(ABSTIME), 0);
    dnp3_p_binoutev_oblock = h_choice(oblock_notime,
                                      oblock_abstime, NULL);
}
