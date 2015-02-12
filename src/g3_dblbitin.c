#include <dnp3.h>
#include <hammer/glue.h>
#include "app.h"
#include "binary.h"

#include "g3_dblbitin.h"


HParser *dnp3_p_dblbitin_rblock;
HParser *dnp3_p_dblbitin_oblock;

void dnp3_p_init_g3_dblbitin(void)
{
    H_RULE(oblock_packed,   dnp3_p_oblock_packed(G(DBLBITIN), V(PACKED), dnp3_p_bin_packed2));
    H_RULE(oblock_flags,    dnp3_p_oblock(G(DBLBITIN), V(FLAGS), dnp3_p_bin_flags2));

    dnp3_p_dblbitin_rblock = dnp3_p_rblock(G(DBLBITIN), V(PACKED), V(FLAGS), 0);
    dnp3_p_dblbitin_oblock = h_choice(oblock_packed, oblock_flags, NULL);
}
