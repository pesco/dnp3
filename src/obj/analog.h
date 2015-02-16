#ifndef DNP3_ANALOG_H_SEEN
#define DNP3_ANALOG_H_SEEN


void dnp3_p_init_analog(void);

extern HParser *dnp3_p_anain_rblock;
extern HParser *dnp3_p_anain_oblock;

extern HParser *dnp3_p_frozenanain_rblock;
extern HParser *dnp3_p_frozenanain_oblock;

extern HParser *dnp3_p_anainev_rblock;
extern HParser *dnp3_p_anainev_oblock;

extern HParser *dnp3_p_frozenanainev_rblock;
extern HParser *dnp3_p_frozenanainev_oblock;


#endif // DNP3_ANALOG_H_SEEN
