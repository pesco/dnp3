#include <dnp3.h>
#include <hammer/glue.h>
#include "app.h"
#include "binary.h"

#include "g2_bininev.h"


HParser *dnp3_p_bininev_rblock;
HParser *dnp3_p_bininev_oblock;

void dnp3_p_init_g2_bininev(void)
{
    H_RULE(oblock_notime,   dnp3_p_oblock(G(BININEV), V(NOTIME), dnp3_p_bin_flags));

    dnp3_p_bininev_rblock = dnp3_p_rblock(G(BININEV),
                                          V(NOTIME), V(ABSTIME), V(RELTIME), 0);
    dnp3_p_bininev_oblock = h_choice(oblock_notime, NULL);
}
