#include <dnp3.h>
#include <hammer/glue.h>
#include "app.h"
#include "binary.h"

#include "g10_binout.h"


HParser *dnp3_p_binout_rblock;
HParser *dnp3_p_binout_wblock;
HParser *dnp3_p_binout_oblock;

void dnp3_p_init_g10_binout(void)
{
    H_RULE(oblock_packed,   dnp3_p_oblock_packed(G(BINOUT), V(PACKED), dnp3_p_bin_packed));
    H_RULE(oblock_flags,    dnp3_p_oblock(G(BINOUT), V(FLAGS), dnp3_p_bin_outflags));

    dnp3_p_binout_rblock = dnp3_p_rblock(G(BINOUT), V(PACKED), V(FLAGS), 0);
    dnp3_p_binout_wblock = oblock_packed;
    dnp3_p_binout_oblock = h_choice(oblock_packed, oblock_flags, NULL);
}

