// sub-parsers for binary object types (packed format and "with flags")

extern HParser *dnp3_p_bin_packed;
extern HParser *dnp3_p_bin_packed2;
extern HParser *dnp3_p_bin_flags;
extern HParser *dnp3_p_bin_flags2;
extern HParser *dnp3_p_bin_outflags;

extern HParser *dnp3_p_abstime;
extern HParser *dnp3_p_reltime;
HParsedToken *dnp3_p_act_abstime(const HParseResult *p, void *user);
HParsedToken *dnp3_p_act_reltime(const HParseResult *p, void *user);

void dnp3_p_init_binary(void);
