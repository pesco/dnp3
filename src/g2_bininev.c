#include <dnp3.h>
#include "app.h"

#include "g2_bininev.h"


HParser *dnp3_p_bininev_rblock;

void dnp3_p_init_g2_bininev(void)
{
    dnp3_p_bininev_rblock = dnp3_p_rblock(G(BININEV),
                                          V(NOTIME), V(ABSTIME), V(RELTIME), 0);
}
