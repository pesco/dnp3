// sub-parsers for binary object types (packed format and "with flags")

extern HParser *dnp3_p_bin_packed;
extern HParser *dnp3_p_bin_flags;

extern HParser *dnp3_p_abstime;
extern HParser *dnp3_p_reltime;

void dnp3_p_init_binary(void);
