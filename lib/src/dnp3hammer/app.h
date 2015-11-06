#ifndef DNP3_APP_H_SEEN
#define DNP3_APP_H_SEEN

// app.h is pretty useless without the oblock combinators
#include "oblock.h"


void dnp3_p_init_app(void);

// short-hands to save some noise in group/variation arguments
#define G(g)     DNP3_GROUP_##g
#define V(g,v)   DNP3_VARIATION_##g##_##v
#define G_V(g,v) G(g),V(g,v)
#define GV(g,v)  (G(g) << 8 | V(g,v))


#endif  // DNP3_APP_H_SEEN
