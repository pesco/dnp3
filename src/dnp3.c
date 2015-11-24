
#include <dnp3hammer.h>

#include "util.h"
#include "app.h"
#include "transport.h"
#include "link.h"

void dnp3_dissector_init(void); // from dissector.c

void dnp3_p_init(void)
{
    dnp3_p_init_util();
    dnp3_p_init_app();
    dnp3_p_init_transport();
    dnp3_p_init_link();
}

// XXX debug
void *h_pprint_lr_info(FILE *f, HParser *p);
void h_pprint_lrtable(FILE *f, void *, void *, int);

void dnp3_init(void)
{
    dnp3_p_init();
    dnp3_dissector_init();

    // XXX debug
#if 0
    void *g = h_pprint_lr_info(stdout, dnp3_p_transport_function);
    assert(g != NULL);
    fprintf(stdout, "\n==== L A L R  T A B L E ====\n");
    h_pprint_lrtable(stdout, g, dnp3_p_transport_function->backend_data, 0);
#endif
}
