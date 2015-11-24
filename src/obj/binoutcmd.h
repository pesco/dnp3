#ifndef DNP3_OBJ_BINOUTCMD_H_SEEN
#define DNP3_OBJ_BINOUTCMD_H_SEEN


extern HParser *dnp3_p_binoutcmdev_rblock;
extern HParser *dnp3_p_binoutcmdev_oblock;

extern HParser *dnp3_p_binoutcmd_rblock;

extern HParser *dnp3_p_g12v1_binoutcmd_crob_oblock;
extern HParser *dnp3_p_g12v2_binoutcmd_pcb_oblock;
extern HParser *dnp3_p_g12v3_binoutcmd_pcm_oblock;
extern HParser *dnp3_p_g12v3_binoutcmd_pcm_rblock;

void dnp3_p_init_binoutcmd(void);


#endif // DNP3_OBJ_BINOUTCMD_H_SEEN
