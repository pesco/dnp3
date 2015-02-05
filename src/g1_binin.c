#include <dnp3.h>
#include "app.h"

#include "g1_binin.h"


HParser *dnp3_p_binin_rblock;

void dnp3_p_init_g1_binin(void)
{
    dnp3_p_binin_rblock = dnp3_p_rblock(G(BININ), V(PACKED), V(FLAGS), 0);
}
