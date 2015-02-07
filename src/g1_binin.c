#include <dnp3.h>
#include <hammer/glue.h>
#include "app.h"
#include "binary.h"

#include "g1_binin.h"


HParser *dnp3_p_binin_rblock;
HParser *dnp3_p_binin_oblock;

void dnp3_p_init_g1_binin(void)
{
    H_RULE(oblock_packed,   dnp3_p_oblock_packed(G(BININ), V(PACKED), dnp3_p_bin_packed));
    H_RULE(oblock_flags,    dnp3_p_oblock(G(BININ), V(FLAGS), dnp3_p_bin_flags));

    dnp3_p_binin_rblock = dnp3_p_rblock(G(BININ), V(PACKED), V(FLAGS), 0);
    dnp3_p_binin_oblock = h_choice(oblock_packed, oblock_flags, NULL);
}
