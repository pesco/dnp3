// misc. utilities

#ifndef DNP3_UTIL_H_SEEN
#define DNP3_UTIL_H_SEEN


// pad with zero bits until the next byte boundary
extern HParser *dnp3_p_pad;

// parse n reserved bits; must be zero, ignored in sequences
HParser *dnp3_p_reserved(size_t n);

// like h_choice but defaults to a TT_ERR_OBJ_UNKNOWN case
HParser *dnp3_p_objchoice(HParser *p, ...);

// like h_many but stops on and propagates TT_ERR and friends
HParser *dnp3_p_many(HParser *p);

// like h_left(p, h_end_p()) but propagates TT_ERR and friends
HParser *dnp3_p_packet(HParser *p);

void dnp3_p_init_util(void);


#endif  // DNP3_UTIL_H_SEEN
