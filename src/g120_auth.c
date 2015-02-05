#include <dnp3.h>
#include <hammer/glue.h>
#include "g120_auth.h"
#include "app.h"

HParser *dnp3_p_g120v3_auth_aggr_block;
HParser *dnp3_p_g120v9_auth_mac_block;

static HParser *auth_mac(size_t n)  // n = size in object prefix
{
    return h_repeat_n(h_uint8(), n);
}

void dnp3_p_init_g120_auth(void)
{
    // A45.3
    H_RULE(seqno,     h_uint32());
    H_RULE(userno,    h_int_range(h_uint16(), 1, 65535));
    H_RULE(auth_aggr, h_sequence(seqno, userno, NULL));

    dnp3_p_g120v3_auth_aggr_block = dnp3_p_single(G(AUTH), V(AGGR), auth_aggr);
    dnp3_p_g120v9_auth_mac_block = dnp3_p_single_vf(G(AUTH), V(MAC), auth_mac);
}
