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

extern HParser *dnp3_p_anaindeadband_rblock;
extern HParser *dnp3_p_anaindeadband_oblock;

extern HParser *dnp3_p_anaoutstatus_rblock;
extern HParser *dnp3_p_anaoutstatus_oblock;

extern HParser *dnp3_p_anaout_sblock;
extern HParser *dnp3_p_anaout_oblock;

extern HParser *dnp3_p_anaoutev_rblock;
extern HParser *dnp3_p_anaoutev_oblock;

extern HParser *dnp3_p_anaoutcmdev_rblock;
extern HParser *dnp3_p_anaoutcmdev_oblock;


#endif // DNP3_ANALOG_H_SEEN
