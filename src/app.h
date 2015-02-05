#ifndef DNP3_APP_H_SEEN
#define DNP3_APP_H_SEEN


void dnp3_p_init_app(void);

// short-hands to save some noise in group/variation arguments
#define G(x) DNP3_GROUP_##x
#define V(x) DNP3_VARIATION_##x
//XXX #define GV(g,v) DNP3_GROUP_##g, DNP3_VARIATION_##v
    // e.g. GV(AUTH,AGGR) -> DNP3_GROUP_AUTH, DNP3_VARIATION_AGGR

// parse an "rblock" of the given group/variations, that is a block of object
// headers and possibly object prefixes, as used in read requests
HParser *dnp3_p_rblock(DNP3_Group g, ...);

// parse an "oblock" of a single object of the given type.
HParser *dnp3_p_single(DNP3_Group g, DNP3_Variation v, HParser *obj);

// parse an "oblock" of a single variable-format object of the given type.
HParser *dnp3_p_single_vf(DNP3_Group g, DNP3_Variation v, HParser *(*obj)(size_t));


#endif  // DNP3_APP_H_SEEN
