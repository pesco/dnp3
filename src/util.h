// misc. utilities

#ifndef DNP3_UTIL_H_SEEN
#define DNP3_UTIL_H_SEEN


// parse n reserved bits; must be zero, ignored in sequences
HParser *dnp3_p_reserved(size_t n);

// like h_choice but defaults to a TT_ERR_OBJ_UNKNOWN case
HParser *dnp3_p_objchoice(HParser *p, ...);

// like h_many but stops on and propagates TT_ERR and friends
HParser *dnp3_p_many(HParser *p);

// like h_left(p, h_end_p()) but propagates TT_ERR and friends
HParser *dnp3_p_packet(HParser *p);

#define ISERR(tt) ((tt) >= TT_ERR && (tt) < TT_USER)



#endif  // DNP3_UTIL_H_SEEN
