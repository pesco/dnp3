// XXX placeholder header for possible extensions to hammer


// parser that always succeeds with the given result token.
HParser *h_unit(const HParsedToken *tok);

// parser that always "succeeds" with the given error code (token type).
HParser *h_error(int code);     // TT_ERR <= code < TT_USER

// helper to construct custom error tokens
// we use (abuse?) the 'user' field to report user-supplied data.
HParsedToken *h_make_err(HArena *arena, HTokenType type, void *value);

// macro to test whether a token type represents an error
#define H_ISERR(tt) ((tt) >= TT_ERR && (tt) < TT_USER)
