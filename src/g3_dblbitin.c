#include <dnp3.h>
#include "app.h"

#include "g3_dblbitin.h"


HParser *dnp3_p_dblbitin_rblock;

void dnp3_p_init_g3_dblbitin(void)
{
    dnp3_p_dblbitin_rblock = dnp3_p_rblock(G(DBLBITIN), V(PACKED), V(FLAGS), 0);
}
