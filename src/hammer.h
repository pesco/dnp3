// XXX placeholder header for possible extensions to hammer


// pad p to consume n-bit chunks, trailing bits matching q. i.e.:
//
// run p, then consume more bits until a multiple of n has been consumed, run q
// on the trailing bits. fail if p or q fails or q doesn't consume all bits.
//
// result: the result of p
HParser *h_pad(int n, const HParser *p, const HParser *q);

// n-bit-align the input stream, skipped bits matching q, i.e.:
//
// consume bits until the input position is a multiple of n bits, then run q on
// the bits. fail if q fails or q doesn't consume all bits.
//
// result: the result of q
HParser *h_align(int n, const HParser *q);

// parser that always succeeds with the given result token.
HParser *h_unit(const HParsedToken *tok);

// parser that always "succeeds" with a result token type of (TT_ERR + code)
HParser *h_error(int code);     // 0 <= code < 32 (XXX auto-clamp?)
