#ifndef DNP3_COUNTER_H_SEEN
#define DNP3_COUNTER_H_SEEN


void dnp3_p_init_counter(void);

extern HParser *dnp3_p_ctr_rblock;
extern HParser *dnp3_p_ctr_oblock;
extern HParser *dnp3_p_frozenctr_rblock;
extern HParser *dnp3_p_frozenctr_oblock;
extern HParser *dnp3_p_ctrev_rblock;
extern HParser *dnp3_p_ctrev_oblock;
extern HParser *dnp3_p_frozenctrev_rblock;
extern HParser *dnp3_p_frozenctrev_oblock;


#endif // DNP3_COUNTER_H_SEEN
