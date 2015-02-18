#ifndef DNP3_TIME_H_SEEN
#define DNP3_TIME_H_SEEN

extern HParser *dnp3_p_g50v1_time_rblock;
extern HParser *dnp3_p_g50v4_indexed_time_rblock;

extern HParser *dnp3_p_g50v1_time_oblock;
extern HParser *dnp3_p_g50v2_time_interval_oblock;
extern HParser *dnp3_p_g50v3_recorded_time_oblock;
extern HParser *dnp3_p_g50v4_indexed_time_oblock;

extern HParser *dnp3_p_cto_oblock;

extern HParser *dnp3_p_delay_oblock;

void dnp3_p_init_time(void);

#endif // DNP3_TIME_H_SEEN
