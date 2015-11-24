#include <dnp3hammer/dnp3.h>

#include <hammer/glue.h>
#include "../app.h"
#include "../util.h"

#include "class.h"


HParser *dnp3_p_g60v1_class0_rblock;
HParser *dnp3_p_g60v2_class1_rblock;
HParser *dnp3_p_g60v3_class2_rblock;
HParser *dnp3_p_g60v4_class3_rblock;


void dnp3_p_init_class(void)
{
    dnp3_p_g60v1_class0_rblock = dnp3_p_rblock_all(G_V(CLASS, 0));
    dnp3_p_g60v2_class1_rblock = dnp3_p_rblock_max(G_V(CLASS, 1));
    dnp3_p_g60v3_class2_rblock = dnp3_p_rblock_max(G_V(CLASS, 2));
    dnp3_p_g60v4_class3_rblock = dnp3_p_rblock_max(G_V(CLASS, 3));
}
