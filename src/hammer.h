// XXX placeholder header for possible extensions to hammer


// parser that always succeeds with the given result token.
HParser *h_unit(const HParsedToken *tok);
HParser *h_unit__m(HAllocator *mm__, const HParsedToken *tok);

// parser that always "succeeds" with the given error code (token type).
HParser *h_error(int code);     // TT_ERR <= code < TT_USER

// helpers to construct custom error tokens
// we use (abuse?) the 'user' and other fields to report user-supplied data.
HParsedToken *h_make_err(HArena *arena, HTokenType type, void *value);
HParsedToken *h_make_err_uint(HArena *arena, HTokenType type, uint64_t value);

// macro to test whether a token type represents an error
#define H_ISERR(tt) ((tt) >= TT_ERR && (tt) < TT_USER)

// parsing IEEE single and double precision floating point numbers
HParser *h_float32(void);
HParser *h_float64(void);

#define TT_FLOAT 9

#define H_ASSERT_FLOAT(TOK) h_assert_type(TT_FLOAT, TOK)
#define H_CAST_FLOAT(TOK) (H_ASSERT_FLOAT(TOK)->dbl)
#define H_INDEX_FLOAT(SEQ, ...) H_CAST_FLOAT(H_INDEX_TOKEN(SEQ, __VA_ARGS__))
#define H_FIELD_FLOAT(...) H_INDEX_FLOAT(p->ast, __VA_ARGS__)
