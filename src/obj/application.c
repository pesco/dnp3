#include <dnp3hammer.h>

#include <hammer/hammer.h>
#include <hammer/glue.h>

#include "../app.h"


HParser *dnp3_p_g90v1_applid_oblock;


static HParsedToken *act_bytes(const HParseResult *p, void *user)
{
    DNP3_Object *o = H_ALLOC(DNP3_Object);
    HCountedArray *a = H_CAST_SEQ(p->ast);
    size_t n = a->used;

    o->applid.len = n;
    o->applid.str = h_arena_malloc(p->arena, n+1);  // null-terminate

    for(size_t i=0; i<n; i++)
        o->applid.str[i] = H_CAST_UINT(a->elements[i]);
    o->applid.str[n] = '\0';

    return H_MAKE(DNP3_Object, o);
}

static HParser *bytes(HAllocator *mm__, size_t n)
{
    return h_action__m(mm__, h_repeat_n__m(mm__, h_uint8__m(mm__), n),
                             act_bytes, NULL);
}

void dnp3_p_init_application(void)
{
    dnp3_p_g90v1_applid_oblock = dnp3_p_oblock_vf(G_V(APPL, ID), bytes);
}
