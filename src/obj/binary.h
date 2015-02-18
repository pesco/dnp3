#ifndef DNP3_BINARY_H_SEEN
#define DNP3_BINARY_H_SEEN


void dnp3_p_init_binary(void);

extern HParser *dnp3_p_binin_rblock;
extern HParser *dnp3_p_binin_oblock;

extern HParser *dnp3_p_bininev_rblock;
extern HParser *dnp3_p_bininev_oblock;

extern HParser *dnp3_p_dblbitin_rblock;
extern HParser *dnp3_p_dblbitin_oblock;

extern HParser *dnp3_p_dblbitinev_rblock;
extern HParser *dnp3_p_dblbitinev_oblock;

extern HParser *dnp3_p_binout_rblock;
extern HParser *dnp3_p_binout_oblock;
extern HParser *dnp3_p_g10v1_binout_packed_oblock;

extern HParser *dnp3_p_binoutev_rblock;
extern HParser *dnp3_p_binoutev_oblock;


#endif // DNP3_BINARY_H_SEEN
