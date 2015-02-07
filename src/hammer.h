// XXX placeholder header for possible extensions to hammer


// parser that always succeeds with the given result token.
HParser *h_unit(const HParsedToken *tok);

// parser that always "succeeds" with the given error code (token type).
HParser *h_error(int code);     // TT_ERR <= code < TT_USER
