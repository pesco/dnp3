// misc. utilities

#ifndef DNP3_UTIL_H_SEEN
#define DNP3_UTIL_H_SEEN

// pad with zero bits until the next byte boundary
extern HParser *dnp3_p_pad;

extern HParser *dnp3_p_dnp3time;    // 48-bit [ms since 1970-01-01]
extern HParser *dnp3_p_reltime;     // 16-bit [ms since CTO]

// parse n reserved bits; must be zero, ignored in sequences
HParser *dnp3_p_reserved(size_t n);

// parse an (unsigned) integer x via parser p
HParser *dnp3_p_int_exact(HParser *p, uint64_t x);

// like h_choice but defaults to a TT_ERR_OBJ_UNKNOWN case
HParser *dnp3_p_objchoice(HParser *p, ...);

// like h_many but stops on and propagates TT_ERR and friends
HParser *dnp3_p_many(HParser *p);

// like h_sequence but stops on and propagates TT_ERR and friends
HParser *dnp3_p_seq(HParser *p, HParser *q);

// like h_left(p, h_end_p()) but propagates TT_ERR and friends
HParser *dnp3_p_packet(HParser *p);

#define little_endian(p)  h_with_endianness(BIT_LITTLE_ENDIAN|BYTE_LITTLE_ENDIAN, p)
#define bit_big_endian(p) h_with_endianness(BIT_BIG_ENDIAN|BYTE_LITTLE_ENDIAN, p)

void dnp3_p_init_util(void);


#endif  // DNP3_UTIL_H_SEEN
