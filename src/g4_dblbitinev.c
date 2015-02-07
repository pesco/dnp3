#include <dnp3.h>
#include "app.h"

#include "g4_dblbitinev.h"


HParser *dnp3_p_dblbitinev_rblock;

void dnp3_p_init_g4_dblbitinev(void)
{
    dnp3_p_dblbitinev_rblock = dnp3_p_rblock(G(DBLBITINEV),
                                             V(NOTIME), V(ABSTIME), V(RELTIME), 0);
}
