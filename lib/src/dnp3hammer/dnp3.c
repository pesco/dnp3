#include <dnp3.h>
#include "util.h"
#include "app.h"
#include "transport.h"
#include "link.h"

void dnp3_p_init(void)
{
    dnp3_p_init_util();
    dnp3_p_init_app();
    dnp3_p_init_transport();
    dnp3_p_init_link();
}
