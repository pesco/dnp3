// object block combinators
//
// an object "block" is an object header followed by the specified objects.
// instead of a generic parser for object headers that determines the following
// object parser, we construct combined block parsers as needed for each type
// of object.
//
// these combinators generally take as arguments the group and variation
// numbers and the "raw" object parser.

#ifndef DNP3_OBLOCK_H_SEEN
#define DNP3_OBLOCK_H_SEEN


void init_oblock(void);

// parse an "rblock" for the given group/variations, that is a block of object
// headers and possibly object prefixes, as used in read requests.
// allows variation 0 ("any").
// the value 0 is also used to terminate the argument list.
HParser *dnp3_p_rblock(DNP3_Group g, ...);

// parse an "rblock" for exactly the given group/variation.
HParser *dnp3_p_specific_rblock(DNP3_Group g, DNP3_Variation v);

// parse an "rblock" for all objects (qc=06) of the given group/variation.
HParser *dnp3_p_rblock_all(DNP3_Group g, DNP3_Variation v);

// parse an "rblock" for all or a maximum number of objects (qc=06-08)
HParser *dnp3_p_rblock_max(DNP3_Group g, DNP3_Variation v);

// parse an "oblock" of a single object of the given type.
HParser *dnp3_p_single(DNP3_Group g, DNP3_Variation v, HParser *obj);

// parse an "rblock" for a single object of the given type.
HParser *dnp3_p_single_rblock(DNP3_Group g, DNP3_Variation v);

// parse an "oblock" of a single variable-format object of the given type.
HParser *dnp3_p_single_vf(DNP3_Group g, DNP3_Variation v, HParser *(*obj)(HAllocator *mm__, size_t));

// parse an "oblock" of objects of the given type.
HParser *dnp3_p_oblock(DNP3_Group g, DNP3_Variation v, HParser *obj);

// like dnp3_p_oblock but only accept range formats, parse objects little-endian,
// and pad with zero bits up to the next byte boundary
HParser *dnp3_p_oblock_packed(DNP3_Group g, DNP3_Variation v, HParser *obj);

// parse an "oblock" of variable-format objects of the given type.
HParser *dnp3_p_oblock_vf(DNP3_Group g, DNP3_Variation v, HParser *(*obj)(HAllocator *mm__, size_t));


#endif // DNP3_OBLOCK_H_SEEN
