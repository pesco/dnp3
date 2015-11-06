#include <dnp3.h>
#include <hammer/hammer.h>
#include <hammer/glue.h>
#include "../app.h"
#include "../util.h"

#include "iin.h"


HParser *dnp3_p_iin_rblock;
HParser *dnp3_p_iin_oblock;

static HParsedToken *act_packed(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    o->bit = H_CAST_UINT(p->ast);
    return H_MAKE(DNP3_Object, o);
}

void dnp3_p_init_iin(void)
{
    H_ARULE(packed,      h_bits(1, false));

    dnp3_p_iin_rblock = dnp3_p_specific_rblock(G_V(IIN, PACKED));
    dnp3_p_iin_oblock = dnp3_p_oblock_packed(G_V(IIN, PACKED), packed);

    // XXX should only certain IIN indexes be allowed with WRITE?!
}
