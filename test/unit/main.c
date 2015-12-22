#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>   // PRIu64
#include <glib.h>

#include <dnp3hammer.h>

#define H_ISERR(tt) ((tt) >= TT_ERR && (tt) < TT_USER)  // XXX


/// test macros (lifted/adapted from hammer's test suite) ///

// a function to format any known parsed token type
static char *format(const HParsedToken *p)
{
    if(!p)
        return h_write_result_unamb(p);

    switch(p->token_type) {
    case TT_DNP3_Fragment:  return dnp3_format_fragment(p->user);
    case TT_DNP3_Segment:   return dnp3_format_segment(p->user);
    case TT_DNP3_Frame:     return dnp3_format_frame(p->user);
    }

    if(H_ISERR(p->token_type)) {
        char *result = NULL;
        char *name = NULL;
        char *frag = NULL;

        switch(p->token_type) {
        case ERR_FUNC_NOT_SUPP: name = g_strdup("FUNC_NOT_SUPP"); break;
        case ERR_OBJ_UNKNOWN:   name = g_strdup("OBJ_UNKNOWN"); break;
        case ERR_PARAM_ERROR:   name = g_strdup("PARAM_ERROR"); break;
        default:
            name = g_strdup_printf("ERROR %d", p->token_type-TT_ERR);
        }

        if(p->user)
            frag = dnp3_format_fragment(p->user);
        else
            frag = g_strdup("-?-");

        result = g_strdup_printf("%s on %s", name, frag);

        free(name);
        free(frag);
        return result;
    }

    return h_write_result_unamb(p);
}

#define check_inttype(fmt, typ, n1, op, n2) do {                        \
    typ _n1 = (n1);                                                     \
    typ _n2 = (n2);                                                     \
    if (!(_n1 op _n2)) {                                                \
      g_test_message("Check failed on line %d: (%s): (" fmt " %s " fmt ")", \
                     LINE, #n1 " " #op " " #n2,                         \
                     _n1, #op, _n2);                                    \
      g_test_fail();                                                    \
    }                                                                   \
  } while(0)

#define check_cmp_size(n1, op, n2) check_inttype("%zu", size_t, n1, op, n2)

#define check_string(n1, op, n2) do {                                   \
    const char *_n1 = (n1);                                             \
    const char *_n2 = (n2);                                             \
    if (!(strcmp(_n1, _n2) op 0)) {                                     \
      g_test_message("Check failed on line %d: (%s) (%s %s %s)",        \
             LINE,                                                      \
             #n1 " " #op " " #n2,                                       \
             _n1, #op, _n2);                                            \
      g_test_fail();                                                    \
    }                                                                   \
  } while(0)

void do_check_parse_fail(const HParser* parser, const uint8_t* input, size_t length, int LINE)
{
    HParseResult *result = h_parse(parser, input, length);
    if (NULL != result) {
        char* cres = format(result->ast);
        g_test_message("Check failed on line %d: shouldn't have succeeded, but parsed %s", LINE, cres);
        free(cres);
        g_test_fail();
        h_parse_result_free(result);
    }
}

void do_check_parse(const HParser* parser, const uint8_t* input, size_t length, const char* result, int LINE) {
    HParseResult *res = h_parse(parser, input, length);
    if (!res) {
      g_test_message("Parse failed on line %d, while expecting %s", LINE, result);
      g_test_fail();
    } else {
      char *cres = format(res->ast);
      check_string(cres, == , result);
      free(cres);
      HArenaStats stats;
      h_allocator_stats(res->arena, &stats);
      g_test_message("Parse used %zd bytes, wasted %zd bytes. "
                     "Inefficiency: %5f%%",
             stats.used, stats.wasted,
             stats.wasted * 100. / (stats.used + stats.wasted));
      h_parse_result_free(res);
    }
}

void do_check_parse_ttonly(const HParser* parser, const uint8_t* input, size_t length, HTokenType expectTT, int LINE) {
    HParseResult *res = h_parse(parser, input, length);
    if (!res) {
        g_test_message("Parse failed on line %d", LINE);
        g_test_fail();
    } else {
        HTokenType tt = res->ast->token_type;
        if(tt != expectTT)
        {
            g_test_message("expected tt %d, but got %d on line %d", expectTT, tt, LINE);
            g_test_fail();
        }
        HArenaStats stats;
        h_allocator_stats(res->arena, &stats);
        g_test_message("Parse used %zd bytes, wasted %zd bytes. "
                               "Inefficiency: %5f%%",
                       stats.used, stats.wasted,
                       stats.wasted * 100. / (stats.used + stats.wasted));
        h_parse_result_free(res);
    }
}

#define check_parse_fail(parser, input, inp_len) do { \
    do_check_parse_fail(parser, (const uint8_t*)  input, inp_len, __LINE__); \
} while(0)


#define check_parse(parser, input, inp_len, result) do { \
    do_check_parse(parser, (const uint8_t*) input, inp_len, result, __LINE__); \
} while(0)

#define check_parse_ttonly(parser, input, inp_len, expectTT) do { \
    do_check_parse_ttonly(parser, (const uint8_t*) input, inp_len, expectTT, __LINE__); \
} while(0)


/// some test cases that produce seg-faults from fuzzing ///

static void test_crash1(void)
{
    // caused an overlong alloc that went unchecked in Hammer

    // group 2, variation 2 w/ qualifer 0x19 == 4 octet count of objects w/ 1 byte index prefix?
    // count = 0x19191919 (large)
    // clearly there isn't enough trailing data
    const char* input = "\xC0\x81\x00\x00\x02\x02\x19\x19\x19\x19\x19\x19\x19\xff\x7f\xff\xff\x01\x01\x81";
    const size_t len = 20;

    check_parse(dnp3_p_app_response, input, len, "PARAM_ERROR on [0] (fir,fin) RESPONSE");
}


/// tests for common DNP3 vulnerabilities ///

static void test_range_overflow(void)
{
    // 2-byte max range
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1E\x02\x01\x00\x00\xFF\xFF", 11,
                "PARAM_ERROR on [0] RESPONSE");

    // 4-byte max range
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1E\x02\x02\x00\x00\x00\x00\xFF\xFF\xFF\xFF", 15,
                "PARAM_ERROR on [0] RESPONSE");
}

static void test_count_of_zero(void)
{
    // count and prefix headers
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x20\x02\x17\x00", 8,
                "PARAM_ERROR on [0] (fir,fin) RESPONSE");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x20\x02\x28\x00\x00", 9,
                "PARAM_ERROR on [0] (fir,fin) RESPONSE");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x20\x02\x39\x00\x00\x00\x00", 11,
                "PARAM_ERROR on [0] (fir,fin) RESPONSE");

    // count headers (0x07, 0x08, 0x09)
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x07\x00",6,
                                    "PARAM_ERROR on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x08\x00\x00",7,
                                    "PARAM_ERROR on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x09\x00\x00\x00\x00",9,
                                    "PARAM_ERROR on [0] (fir,fin) READ");
}

static void test_mult_overflow(void)
{
    // make sure that the parser doesn't perform a multiplication overflow when parsing count and prefix fields

    // g32v2 has a size of 3 byte. 3 + 4 bytes prefix = 7
    // 0x24924925 * 7 = 3 in 32-bit multiplication overflow

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x20\x02\x39\x25\x49\x92\x24\x01\xAA\xAA", 14,
                "PARAM_ERROR on [0] (fir,fin) RESPONSE");
}

static void test_large_packed_binary(void)
{
    // the purpose of this test is to see how well the parser handles a worst case message in terms
    // of AST size.

    // that means we have 2048 - 11 = 2037 bytes remaining * 8 = 16296 packed binaries
    // so the range is 0 to 16295 == 0x3FA7

    // the header w/o any packed binaries contains 11 bytes
    uint8_t asdu[2011] = {0xC0, 0x81, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x80, 0x3E};

    // w/ 2000 bytes left over for payload, we can shove in 2000*8 = 16K packed binary values, so 1 to 16000 (0x3E80) range

    for(int i=11; i < 2011; ++i) // set all remaining packed bits to "true"
    {
        asdu[i] = 0xFF;
    }

    check_parse_ttonly(dnp3_p_app_response, asdu, 2011, TT_DNP3_Fragment);
}


/// test cases ///

static void test_app_fragment(void)
{
    check_parse_fail(dnp3_p_app_fragment, "",0);
    check_parse_fail(dnp3_p_app_fragment, "\xC0",1);

    check_parse(dnp3_p_app_fragment, "\xC2\x00",2, "[2] (fir,fin) CONFIRM");
    check_parse(dnp3_p_app_fragment, "\xC0\x02\x02\x02\x06",5, "OBJ_UNKNOWN on [0] (fir,fin) WRITE");
    check_parse(dnp3_p_app_fragment, "\xC2\x81\x00\x00",4, "[2] (fir,fin) RESPONSE");

    check_parse(dnp3_p_app_fragment, "\xC0\xFF",2, "FUNC_NOT_SUPP on [0] (fir,fin) 0xFF");
    check_parse(dnp3_p_app_fragment, "\xC0\xFF\x00\x00",4, "FUNC_NOT_SUPP on [0] (fir,fin) 0xFF");
    check_parse(dnp3_p_app_fragment, "\xC0\x01\x01",3, "PARAM_ERROR on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_fragment, "\xC0\x01\x32\x00\x06",5, "OBJ_UNKNOWN on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_fragment, "\xC0\x81\x00\x00\x02\x01\x17\x01\x03\xC3",10,
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");
}

static void test_req_fail(void)
{
    check_parse_fail(dnp3_p_app_request, "",0);
    check_parse_fail(dnp3_p_app_request, "\xC0",1);
    check_parse(dnp3_p_app_request, "\xC0\x23",2, "FUNC_NOT_SUPP on [0] (fir,fin) 0x23");
    check_parse(dnp3_p_app_request, "\xC0\x81",2, "FUNC_NOT_SUPP on [0] (fir,fin) RESPONSE");
}

static void test_req_ac(void)
{
    check_parse(dnp3_p_app_request, "\xC2\x00",2, "[2] (fir,fin) CONFIRM");
    check_parse(dnp3_p_app_request, "\xC5\x00",2, "[5] (fir,fin) CONFIRM");
    check_parse(dnp3_p_app_request, "\xD3\x00",2, "[3] (fir,fin,uns) CONFIRM");
    // XXX is it correct to fail the parse on invalid AC flags?
    check_parse_fail(dnp3_p_app_request, "\xE0\x23",2); // (con) ???
    check_parse_fail(dnp3_p_app_request, "\xE0\x81",2); // (con) RESPONSE
    check_parse(dnp3_p_app_request, "\xD0\x23",2, "FUNC_NOT_SUPP on [0] (fir,fin,uns) 0x23");
    check_parse(dnp3_p_app_request, "\xD0\x81",2, "FUNC_NOT_SUPP on [0] (fir,fin,uns) RESPONSE");
    check_parse_fail(dnp3_p_app_request, "\xE0\x23",2); // (con) ???
    check_parse_fail(dnp3_p_app_request, "\x80\x23",2); // not (fir) ???
    check_parse_fail(dnp3_p_app_request, "\xD0\x01",2); // (uns) READ
    check_parse_fail(dnp3_p_app_request, "\x40\x00",2); // not (fin)
    check_parse_fail(dnp3_p_app_request, "\x80\x00",2); // not (fir)
    check_parse_fail(dnp3_p_app_request, "\xE0\x00",2); // (con) CONFIRM
    check_parse_fail(dnp3_p_app_request, "\xF0\x00",2); // (con,uns)
    check_parse(dnp3_p_app_request, "\xC0\x00",2, "[0] (fir,fin) CONFIRM");
}

static void test_req_ohdr(void)
{
    // truncated (otherwise valid) object header
    check_parse(dnp3_p_app_request, "\xC0\x01\x01",3, "PARAM_ERROR on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00",4, "PARAM_ERROR on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17",5, "PARAM_ERROR on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17\x01",6, "PARAM_ERROR on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x7A",5, "PARAM_ERROR on [0] (fir,fin) READ");

    // truncated object header (invalid group)
    check_parse(dnp3_p_app_request, "\xC0\x01\x05",3, "PARAM_ERROR on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x05\x00",4, "PARAM_ERROR on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x00\x03\x41\x58",8, "PARAM_ERROR on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x06\x05\x00",7, "PARAM_ERROR on [0] (fir,fin) READ");

    // truncated object header (invalid variation)
    check_parse(dnp3_p_app_request, "\xC0\x01\x32\x00",4, "PARAM_ERROR on [0] (fir,fin) READ");

    // invalid group / variation (complete header)
    check_parse(dnp3_p_app_request, "\xC0\x01\x05\x00\x00\x03\x41",7, "OBJ_UNKNOWN on [0] (fir,fin) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x32\x00\x06",5, "OBJ_UNKNOWN on [0] (fir,fin) READ");
}

static void test_req_confirm(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x00",2, "[0] (fir,fin) CONFIRM");
    check_parse(dnp3_p_app_request, "\xD0\x00",2, "[0] (fir,fin,uns) CONFIRM");
    check_parse(dnp3_p_app_request, "\xC0\x00\x01\x00\x06",5, "PARAM_ERROR on [0] (fir,fin) CONFIRM");
        // XXX should a CONFIRM message with unexpected objects yield OBJ_UNKNOWN?
}

static void test_req_read(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01",2, "[0] (fir,fin) READ");  // XXX null READ valid?
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17\x03\x41\x43\x42",9,
                                    "[0] (fir,fin) READ {g1v0 qc=17 #65 #67 #66}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x00\x03\x41",7,
                                    "[0] (fir,fin) READ {g1v0 qc=00 #3..65}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x02\x03\x00\x03\x41",7,
                                    "[0] (fir,fin) READ {g2v3 qc=00 #3..65}");

    // 0 is not a valid count
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17\x00",6,
                                    "PARAM_ERROR on [0] (fir,fin) READ");
}

static void test_req_write(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x02",2, "[0] (fir,fin) WRITE"); // XXX null WRITE valid?
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x00\x00\x03\x06",7, "OBJ_UNKNOWN on [1] (fir,fin) WRITE");
        // (variation 0 ("any") specified - not valid for writes)
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x01\x00\x03\x06\x0E",8,
                                    "[1] (fir,fin) WRITE {g10v1 qc=00 #3..6: 0 1 1 1}");

    // examples given in IEEE 1815-2012 (subclause 4.4.3.2)
    check_parse(dnp3_p_app_request, "\xC3\x02\x50\x01\x00\x07\x07\x00",8,
                                    "[3] (fir,fin) WRITE {g80v1 qc=00 #7..7: 0}");
    check_parse(dnp3_p_app_request, "\xC3\x02\x22\x01\x17\x03\x06\x12\x00\x08\x4A\x00\x14\xFF\xFF",15,
                                    "[3] (fir,fin) WRITE {g34v1 qc=17 #6:18 #8:74 #20:65535}");
    check_parse(dnp3_p_app_request, "\xC3\x02\x32\x01\x07\x01\xAC\xE9\x00\x40\x08\x01",12,
                                    "[3] (fir,fin) WRITE {g50v1 qc=07 @1134945167.788s}");
}

static void test_req_select(void)
{
    // examples given in IEEE 1815-2012 (subclause 4.4.4.4)
    check_parse(dnp3_p_app_request,  "\xC3\x03\x0C\x01\x17\x01\x0A\x41\x01\xFA\x00\x00\x00\x00\x00\x00\x00\x00",18,
                                     "[3] (fir,fin) SELECT {g12v1 qc=17 #10:(CLOSE PULSE_ON 1x on=250ms off=0ms)}");
    check_parse(dnp3_p_app_response, "\xC3\x81\x00\x00\x0C\x01\x17\x01\x0A\x41\x01\xFA\x00\x00\x00\x00\x00\x00\x00\x00",20,
                                     "[3] (fir,fin) RESPONSE {g12v1 qc=17 #10:(CLOSE PULSE_ON 1x on=250ms off=0ms)}");
    check_parse(dnp3_p_app_request,  "\xC3\x03\x0C\x02\x07\x01\x41\x03\xF4\x01\x00\x00\xD0\x07\x00\x00\x00"
                                             "\x0C\x03\x00\x05\x0F\x21\x04",24,
                                     "[3] (fir,fin) SELECT {g12v2 qc=07 (CLOSE PULSE_ON 3x on=500ms off=2000ms)}"
                                                         " {g12v3 qc=00 #5..15: 1 0 0 0 0 1 0 0 0 0 1}");
    check_parse(dnp3_p_app_response, "\xC3\x81\x00\x04\x0C\x02\x07\x01\x41\x03\xF4\x01\x00\x00\xD0\x07\x00\x00\x04"
                                                     "\x0C\x03\x00\x05\x0F",24,
                                     "[3] (fir,fin) RESPONSE (param_error) {g12v2 qc=07 (CLOSE PULSE_ON 3x on=500ms off=2000ms status=4)}"
                                                         " {g12v3 qc=00 #5..15}");

    // mixing CROBs, analog output, and PCBs
    check_parse(dnp3_p_app_request,  "\xC3\x03\x0C\x02\x07\x01\x41\x03\xF4\x01\x00\x00\xD0\x07\x00\x00\x00"
                                             "\x0C\x03\x00\x05\x0F\x21\x04"
                                             "\x29\x01\x17\x01\x01\x12\x34\x56\x78\x00",34,
                                     "[3] (fir,fin) SELECT {g12v2 qc=07 (CLOSE PULSE_ON 3x on=500ms off=2000ms)}"
                                                         " {g12v3 qc=00 #5..15: 1 0 0 0 0 1 0 0 0 0 1}"
                                                         " {g41v1 qc=17 #1:2018915346}");
    check_parse(dnp3_p_app_request,  "\xC3\x03\x0C\x01\x17\x01\x0A\x41\x01\xFA\x00\x00\x00\x00\x00\x00\x00\x00"
                                             "\x0C\x02\x07\x01\x41\x03\xF4\x01\x00\x00\xD0\x07\x00\x00\x00"
                                             "\x0C\x03\x00\x05\x0F\x21\x04"
                                             "\x29\x01\x17\x01\x01\x12\x34\x56\x78\x00",50,
                                     "[3] (fir,fin) SELECT {g12v1 qc=17 #10:(CLOSE PULSE_ON 1x on=250ms off=0ms)}"
                                                         " {g12v2 qc=07 (CLOSE PULSE_ON 3x on=500ms off=2000ms)}"
                                                         " {g12v3 qc=00 #5..15: 1 0 0 0 0 1 0 0 0 0 1}"
                                                         " {g41v1 qc=17 #1:2018915346}");

    // error cases...
    // unexpected object type
    check_parse(dnp3_p_app_request,  "\xC3\x03\x01\x01\x00\x03\x08\x00",8,
                                     "OBJ_UNKNOWN on [3] (fir,fin) SELECT");
    // missing PCM
    check_parse(dnp3_p_app_request,  "\xC3\x03\x0C\x02\x07\x01\x41\x03\xF4\x01\x00\x00\xD0\x07\x00\x00\x00",17,
                                     "PARAM_ERROR on [3] (fir,fin) SELECT");
    // corrupt PCM
    check_parse(dnp3_p_app_request,  "\xC3\x03\x0C\x02\x07\x01\x41\x03\xF4\x01\x00\x00\xD0\x07\x00\x00\x00"
                                             "\x0C\x03\x00\x05\x0F\x21\x84",24, // padding bit set
                                     "PARAM_ERROR on [3] (fir,fin) SELECT");
    // unexpected object in place of PCM
    check_parse(dnp3_p_app_request,  "\xC3\x03\x0C\x02\x07\x01\x41\x03\xF4\x01\x00\x00\xD0\x07\x00\x00\x00"
                                             "\x01\x01\x00\x03\x08\x00",23,
                                     "PARAM_ERROR on [3] (fir,fin) SELECT");
}


static void test_req_select_float(void)
{
    // 0x0000A03F is a little endian representation of 1.25 in IEEE-754 float
    check_parse(dnp3_p_app_request,  "\xC0\x03\x29\x03\x17\x01\x02\x00\x00\xA0\x3F\x00", 12,
                "[0] (fir,fin) SELECT {g41v3 qc=17 #2:1.2}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x29\x03\x17\x01\x02\x00\x00\xA0\x3F\x00", 14,
                "[0] (fir,fin) RESPONSE {g41v3 qc=17 #2:1.2}");

    // 0x000000000000F43F is a little endian representation of 1.25 in IEEE-754 double
    check_parse(dnp3_p_app_request,  "\xC0\x03\x29\x04\x17\x01\x02\x00\x00\x00\x00\x00\x00\xF4\x3F\x00", 16,
                "[0] (fir,fin) SELECT {g41v4 qc=17 #2:1.2}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x29\x04\x17\x01\x02\x00\x00\x00\x00\x00\x00\xF4\x3F\x00", 18,
                "[0] (fir,fin) RESPONSE {g41v4 qc=17 #2:1.2}");

}

static void test_req_operate(void)
{
    // examples given in IEEE 1815-2012 (subclause 4.4.4.4)
    check_parse(dnp3_p_app_request,  "\xC4\x04\x0C\x01\x17\x01\x0A\x41\x01\xFA\x00\x00\x00\x00\x00\x00\x00\x00",18,
                                     "[4] (fir,fin) OPERATE {g12v1 qc=17 #10:(CLOSE PULSE_ON 1x on=250ms off=0ms)}");
    check_parse(dnp3_p_app_response, "\xC4\x81\x00\x00\x0C\x01\x17\x01\x0A\x41\x01\xFA\x00\x00\x00\x00\x00\x00\x00\x00",20,
                                     "[4] (fir,fin) RESPONSE {g12v1 qc=17 #10:(CLOSE PULSE_ON 1x on=250ms off=0ms)}");
}

static void test_req_direct_operate(void)
{
    check_parse(dnp3_p_app_request,  "\xC4\x05\x0C\x01\x17\x01\x0A\x41\x01\xFA\x00\x00\x00\x00\x00\x00\x00\x00",18,
                                     "[4] (fir,fin) DIRECT_OPERATE {g12v1 qc=17 #10:(CLOSE PULSE_ON 1x on=250ms off=0ms)}");
}

static void test_req_direct_operate_nr(void)
{
    check_parse(dnp3_p_app_request,  "\xC4\x06\x0C\x01\x17\x01\x0A\x41\x01\xFA\x00\x00\x00\x00\x00\x00\x00\x00",18,
                                     "[4] (fir,fin) DIRECT_OPERATE_NR {g12v1 qc=17 #10:(CLOSE PULSE_ON 1x on=250ms off=0ms)}");
}

static void test_req_freeze(void)
{
    // counters
    check_parse(dnp3_p_app_request,  "\xC3\x07\x14\x00\x06",5, "[3] (fir,fin) IMMED_FREEZE {g20v0 qc=06}");
    check_parse(dnp3_p_app_request,  "\xC3\x08\x14\x00\x06",5, "[3] (fir,fin) IMMED_FREEZE_NR {g20v0 qc=06}");

    // analog inputs
    check_parse(dnp3_p_app_request,  "\xC3\x07\x1E\x00\x06",5, "[3] (fir,fin) IMMED_FREEZE {g30v0 qc=06}");
    check_parse(dnp3_p_app_request,  "\xC3\x08\x1E\x00\x06",5, "[3] (fir,fin) IMMED_FREEZE_NR {g30v0 qc=06}");
}

static void test_req_freeze_clear(void)
{
    check_parse(dnp3_p_app_request,  "\xC3\x09\x14\x00\x06",5, "[3] (fir,fin) FREEZE_CLEAR {g20v0 qc=06}");
    check_parse(dnp3_p_app_request,  "\xC3\x0A\x14\x00\x06",5, "[3] (fir,fin) FREEZE_CLEAR_NR {g20v0 qc=06}");

    // only counters can be cleared
    check_parse(dnp3_p_app_request,  "\xC3\x09\x1E\x00\x06",5, "OBJ_UNKNOWN on [3] (fir,fin) FREEZE_CLEAR");
    check_parse(dnp3_p_app_request,  "\xC3\x0A\x1E\x00\x06",5, "OBJ_UNKNOWN on [3] (fir,fin) FREEZE_CLEAR_NR");
}

static void test_req_freeze_at_time(void)
{
    // counters
    check_parse(dnp3_p_app_request,  "\xC3\x0B\x32\x02\x07\x01\xC0\x5F\x63\x1C\xE7\x00\xA0\xBB\x0D\x00\x14\x00\x06",19,
                                     "[3] (fir,fin) FREEZE_AT_TIME {g50v2 qc=07 @992613720s+900s} {g20v0 qc=06}");
    check_parse(dnp3_p_app_request,  "\xC3\x0C\x32\x02\x07\x01\xC0\x5F\x63\x1C\xE7\x00\xA0\xBB\x0D\x00\x14\x00\x06",19,
                                     "[3] (fir,fin) FREEZE_AT_TIME_NR {g50v2 qc=07 @992613720s+900s} {g20v0 qc=06}");

    // analog inputs
    check_parse(dnp3_p_app_request,  "\xC3\x0B\x32\x02\x07\x01\xC0\x5F\x63\x1C\xE7\x00\xA0\xBB\x0D\x00\x1E\x00\x06",19,
                                     "[3] (fir,fin) FREEZE_AT_TIME {g50v2 qc=07 @992613720s+900s} {g30v0 qc=06}");
    check_parse(dnp3_p_app_request,  "\xC3\x0C\x32\x02\x07\x01\xC0\x5F\x63\x1C\xE7\x00\xA0\xBB\x0D\x00\x1E\x00\x06",19,
                                     "[3] (fir,fin) FREEZE_AT_TIME_NR {g50v2 qc=07 @992613720s+900s} {g30v0 qc=06}");
}

static void test_req_restart(void)
{
    check_parse(dnp3_p_app_request,  "\xC3\x0D",2, "[3] (fir,fin) COLD_RESTART");
    check_parse(dnp3_p_app_request,  "\xC3\x0E",2, "[3] (fir,fin) WARM_RESTART");
}

static void test_req_initialize_data(void)
{
    // obsolete
    check_parse(dnp3_p_app_request,  "\xC3\x0F",2, "FUNC_NOT_SUPP on [3] (fir,fin) INITIALIZE_DATA");
}

static void test_req_application(void)
{
    check_parse(dnp3_p_app_request,  "\xC3\x10\x5A\x01\x5B\x01\x03\x00\x43\x4C\x36",11,
                                     "[3] (fir,fin) INITIALIZE_APPL {g90v1 qc=5B 'CL6'}");
    check_parse(dnp3_p_app_request,  "\xC3\x11\x5A\x01\x5B\x01\x03\x00\x43\x4C\x36",11,
                                     "[3] (fir,fin) START_APPL {g90v1 qc=5B 'CL6'}");
    check_parse(dnp3_p_app_request,  "\xC3\x12\x5A\x01\x5B\x01\x03\x00\x43\x4C\x36",11,
                                     "[3] (fir,fin) STOP_APPL {g90v1 qc=5B 'CL6'}");
}

static void test_req_enable_unsolicited(void)
{
    check_parse(dnp3_p_app_request,  "\xC3\x14\x3C\x02\x06\x3C\x03\x06\x3C\x04\x06",11,
                                     "[3] (fir,fin) ENABLE_UNSOLICITED {g60v2 qc=06} {g60v3 qc=06} {g60v4 qc=06}");
}

static void test_req_disable_unsolicited(void)
{
    check_parse(dnp3_p_app_request,  "\xC3\x15\x3C\x02\x06\x3C\x03\x06\x3C\x04\x06",11,
                                     "[3] (fir,fin) DISABLE_UNSOLICITED {g60v2 qc=06} {g60v3 qc=06} {g60v4 qc=06}");
}

static void test_req_assign_class(void)
{
    check_parse(dnp3_p_app_request,  "\xC3\x16\x3C\x02\x06\x01\x00\x06"
                                             "\x3C\x03\x06\x1E\x00\x06\x15\x00\x00\x00\x02"
                                             "\x3C\x01\x06\x15\x00\x00\x03\x0A",27,
                                     "[3] (fir,fin) ASSIGN_CLASS {g60v2 qc=06} {g1v0 qc=06}"
                                                               " {g60v3 qc=06} {g30v0 qc=06} {g21v0 qc=00 #0..2}"
                                                               " {g60v1 qc=06} {g21v0 qc=00 #3..10}");
}

static void test_req_delay_measure(void)
{
    check_parse(dnp3_p_app_request,  "\xC3\x17",2, "[3] (fir,fin) DELAY_MEASURE");
}

static void test_req_record_current_time(void)
{
    check_parse(dnp3_p_app_request,  "\xC3\x18",2, "[3] (fir,fin) RECORD_CURRENT_TIME");
}

static void test_rsp_fail(void)
{
    check_parse_fail(dnp3_p_app_response, "",0);
    check_parse_fail(dnp3_p_app_response, "\xC2",1);
    check_parse_fail(dnp3_p_app_response, "\xC2\x81",2);
    check_parse_fail(dnp3_p_app_response, "\xC2\x81\x00",3);
    check_parse_fail(dnp3_p_app_response, "\xC0\x00",2);
    check_parse_fail(dnp3_p_app_response, "\xC0\x01",2);
    check_parse_fail(dnp3_p_app_response, "\xC0\x23",2);
    check_parse_fail(dnp3_p_app_response, "\xC0\xF0",2);
    check_parse(dnp3_p_app_response, "\xC2\x00\x00\x00",4, "FUNC_NOT_SUPP on [2] (fir,fin) CONFIRM");
    check_parse(dnp3_p_app_response, "\xC3\x01\x00\x00",4, "FUNC_NOT_SUPP on [3] (fir,fin) READ");
    check_parse(dnp3_p_app_response, "\xC0\x23\x00\x00",4, "FUNC_NOT_SUPP on [0] (fir,fin) 0x23");
    check_parse(dnp3_p_app_response, "\xC0\xF0\x00\x00",4, "FUNC_NOT_SUPP on [0] (fir,fin) 0xF0");
}

static void test_rsp_ac(void)
{
    check_parse(dnp3_p_app_response, "\x02\x81\x00\x00",4, "[2] RESPONSE");
    check_parse(dnp3_p_app_response, "\x22\x81\x00\x00",4, "[2] (con) RESPONSE");
    check_parse(dnp3_p_app_response, "\x83\x81\x00\x00",4, "[3] (fir) RESPONSE");
    check_parse(dnp3_p_app_response, "\xA2\x81\x00\x00",4, "[2] (fir,con) RESPONSE");
    check_parse(dnp3_p_app_response, "\x42\x81\x00\x00",4, "[2] (fin) RESPONSE");
    check_parse(dnp3_p_app_response, "\x64\x81\x00\x00",4, "[4] (fin,con) RESPONSE");
    check_parse(dnp3_p_app_response, "\xC2\x81\x00\x00",4, "[2] (fir,fin) RESPONSE");
    check_parse(dnp3_p_app_response, "\xE2\x81\x00\x00",4, "[2] (fir,fin,con) RESPONSE");

    check_parse(dnp3_p_app_response, "\x32\x82\x00\x00",4, "[2] (con,uns) UNSOLICITED_RESPONSE");
    check_parse(dnp3_p_app_response, "\xB2\x82\x00\x00",4, "[2] (fir,con,uns) UNSOLICITED_RESPONSE");
    check_parse(dnp3_p_app_response, "\x72\x82\x00\x00",4, "[2] (fin,con,uns) UNSOLICITED_RESPONSE");
    check_parse(dnp3_p_app_response, "\xF2\x82\x00\x00",4, "[2] (fir,fin,con,uns) UNSOLICITED_RESPONSE");

    // XXX should unsol. responses with con=0 really be discarded?
    check_parse_fail(dnp3_p_app_response, "\x13\x82\x00\x00",4); // (uns)
    check_parse_fail(dnp3_p_app_response, "\x54\x82\x00\x00",4); // (fir,uns)
    check_parse_fail(dnp3_p_app_response, "\x9A\x82\x00\x00",4); // (fin,uns)
    check_parse_fail(dnp3_p_app_response, "\xD0\x82\x00\x00",4); // (fir,fin,uns)

    check_parse_fail(dnp3_p_app_response, "\x13\x81\x00\x00",4); // (uns)
    check_parse_fail(dnp3_p_app_response, "\x32\x81\x00\x00",4); // (con,uns)
    check_parse_fail(dnp3_p_app_response, "\x54\x81\x00\x00",4); // (fir,uns)
    check_parse_fail(dnp3_p_app_response, "\x72\x81\x00\x00",4); // (fir,con,uns)
    check_parse_fail(dnp3_p_app_response, "\x9A\x81\x00\x00",4); // (fin,uns)
    check_parse_fail(dnp3_p_app_response, "\xB2\x81\x00\x00",4); // (fin,con,uns)
    check_parse_fail(dnp3_p_app_response, "\xD0\x81\x00\x00",4); // (fir,fin,uns)
    check_parse_fail(dnp3_p_app_response, "\xF2\x81\x00\x00",4); // (fir,fin,con,uns)
}

static void test_rsp_iin(void)
{
    check_parse(dnp3_p_app_response, "\x02\x81\x00\x00",4, "[2] RESPONSE");
    check_parse(dnp3_p_app_response, "\x02\x81\x01\x00",4, "[2] RESPONSE (broadcast)");
    check_parse(dnp3_p_app_response, "\x02\x81\x06\x00",4, "[2] RESPONSE (class1,class2)");
    check_parse(dnp3_p_app_response, "\x02\x81\x00\x01",4, "[2] RESPONSE (func_not_supp)");
    check_parse(dnp3_p_app_response, "\x02\x81\x00\x10",4, "[2] RESPONSE (already_executing)");
    check_parse(dnp3_p_app_response, "\x02\x81\x00\x20",4, "[2] RESPONSE (config_corrupt)");
    check_parse(dnp3_p_app_response, "\x02\x81\x80\x20",4, "[2] RESPONSE (device_restart,config_corrupt)");
    check_parse_fail(dnp3_p_app_response, "\x02\x81\x00\x40",4);
    check_parse_fail(dnp3_p_app_response, "\x02\x81\x00\x80",4);
}

static void test_rsp_null(void)
{
    check_parse(dnp3_p_app_response, "\xC2\x81\x00\x00",4, "[2] (fir,fin) RESPONSE");
}

static void test_rsp_unsolicited(void)
{
    check_parse(dnp3_p_app_response, "\x30\x82\x00\x00\x33\x01\x07\x01\x00\x04\x00\x00\x00\x00",14,
                                     "[0] (con,uns) UNSOLICITED_RESPONSE {g51v1 qc=07 @1.024s}");
    check_parse(dnp3_p_app_response, "\x30\x82\x00\x00\x33\x02\x07\x01\x00\x04\x00\x00\x00\x00",14,
                                     "[0] (con,uns) UNSOLICITED_RESPONSE {g51v2 qc=07 (unsynchronized)@1.024s}");
}

static void test_obj_binin(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g1v0 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17\x02\x03\x08",8,
                                    "[0] (fir,fin) READ {g1v0 qc=17 #3 #8}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x01\x00\x03\x08\x19",10,
                                     "[0] (fir,fin) RESPONSE {g1v1 qc=00 #3..8: 1 0 0 1 1 0}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x01\x17\x00",8,          // invalid qc
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x02\x17\x01\x03\x80",10,
                                     "[0] (fir,fin) RESPONSE {g1v2 qc=17 #3:1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x02\x17\x01\x03\x83",10,
                                     "[0] (fir,fin) RESPONSE {g1v2 qc=17 #3:(online,restart)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x02\x17\x01\x03\x89",10,
                                     "[0] (fir,fin) RESPONSE {g1v2 qc=17 #3:(online,remote_forced)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x02\x17\x01\x03\x40",10,  // reserved bit set
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");
}

static void test_obj_bininev(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01\x02\x00\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g2v0 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x02\x01\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g2v1 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x02\x03\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g2v3 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x02\x04\x00\x03\x08",7,
                                    "OBJ_UNKNOWN on [0] (fir,fin) READ");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x01\x17\x01\x03\x80",10,
                                     "[0] (fir,fin) RESPONSE {g2v1 qc=17 #3:1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x01\x17\x01\x03\x83",10,
                                     "[0] (fir,fin) RESPONSE {g2v1 qc=17 #3:(online,restart)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x01\x17\x01\x03\xC3",10,
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x02\x17\x01\x03\x80\x00\x00\x00\x00\x00\x00",16,
                                     "[0] (fir,fin) RESPONSE {g2v2 qc=17 #3:1@0s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x02\x17\x01\x03\x01\x00\x00\x00\x00\x00\x80",16,
                                     "[0] (fir,fin) RESPONSE {g2v2 qc=17 #3:(online)0@140737488355.328s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x02\x17\x01\x03\x82\xA0\xFC\x7D\x7A\x4B\x01",16,
                                     "[0] (fir,fin) RESPONSE {g2v2 qc=17 #3:(restart)1@1423689252s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x02\x17\x01\x03\xC2\x00\x00\x00\x00\x00\x00",16,
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x03\x17\x01\x03\x80\x00\x00",12,
                                     "[0] (fir,fin) RESPONSE {g2v3 qc=17 #3:1@+0s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x03\x17\x01\x03\x01\x00\x80",12,
                                     "[0] (fir,fin) RESPONSE {g2v3 qc=17 #3:(online)0@+32.768s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x03\x17\x01\x03\x81\xE0\x56",12,
                                     "[0] (fir,fin) RESPONSE {g2v3 qc=17 #3:(online)1@+22.240s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x03\x17\x01\x03\xC1\x00\x00",12,
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");
        // XXX should the relative time variants generate a PARAM_ERROR unless they are preceded by a
        //     Common Time-of-Occurance (CTO, group 50) object in the same message?
}

static void test_obj_dblbitin(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01\x03\x00\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g3v0 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x03\x01\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g3v1 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x03\x02\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g3v2 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x03\x03\x00\x03\x08",7,
                                    "OBJ_UNKNOWN on [0] (fir,fin) READ");

    // v1 (packed)
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x01\x00\x00\x03\x36",10,
                                     "[0] (fir,fin) RESPONSE {g3v1 qc=00 #0..3: 1 0 - ~}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x01\x00\x00\x02\x06",10,
                                     "[0] (fir,fin) RESPONSE {g3v1 qc=00 #0..2: 1 0 ~}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x01\x00\x00\x02\x46",10,
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");  // extra bit set

    // v2 (flags)
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x02\x17\x01\x03\x80",10,
                                     "[0] (fir,fin) RESPONSE {g3v2 qc=17 #3:1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x02\x17\x01\x03\x03",10,
                                     "[0] (fir,fin) RESPONSE {g3v2 qc=17 #3:(online,restart)~}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x02\x17\x01\x03\xA1",10,
                                     "[0] (fir,fin) RESPONSE {g3v2 qc=17 #3:(online,chatter_filter)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x02\x17\x01\x03\xC1",10,
                                     "[0] (fir,fin) RESPONSE {g3v2 qc=17 #3:(online)-}");
}

static void test_obj_dblbitinev(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01\x04\x00\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g4v0 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x04\x01\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g4v1 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x04\x03\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g4v3 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x04\x04\x00\x03\x08",7,
                                    "OBJ_UNKNOWN on [0] (fir,fin) READ");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x01\x17\x01\x03\x80",10,
                                     "[0] (fir,fin) RESPONSE {g4v1 qc=17 #3:1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x01\x17\x01\x03\x03",10,
                                     "[0] (fir,fin) RESPONSE {g4v1 qc=17 #3:(online,restart)~}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x01\x17\x01\x03\xC1",10,
                                     "[0] (fir,fin) RESPONSE {g4v1 qc=17 #3:(online)-}");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x02\x17\x01\x03\x80\x00\x00\x00\x00\x00\x00",16,
                                     "[0] (fir,fin) RESPONSE {g4v2 qc=17 #3:1@0s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x02\x17\x01\x03\x41\x00\x00\x00\x00\x00\x80",16,
                                     "[0] (fir,fin) RESPONSE {g4v2 qc=17 #3:(online)0@140737488355.328s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x02\x17\x01\x03\x02\xA0\xFC\x7D\x7A\x4B\x01",16,
                                     "[0] (fir,fin) RESPONSE {g4v2 qc=17 #3:(restart)~@1423689252s}");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x03\x17\x01\x03\x80\x00\x00",12,
                                     "[0] (fir,fin) RESPONSE {g4v3 qc=17 #3:1@+0s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x03\x17\x01\x03\x41\x00\x80",12,
                                     "[0] (fir,fin) RESPONSE {g4v3 qc=17 #3:(online)0@+32.768s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x03\x17\x01\x03\x81\xE0\x56",12,
                                     "[0] (fir,fin) RESPONSE {g4v3 qc=17 #3:(online)1@+22.240s}");
        // XXX should the relative time variants generate a PARAM_ERROR unless they are preceded by a
        //     Common Time-of-Occurance (CTO, group 50) object in the same message?
}

static void test_obj_binout(void)
{
    // v1 (packed)
    check_parse(dnp3_p_app_request, "\xC0\x01\x0A\x01\x00\x03\x41",7,
                                    "[0] (fir,fin) READ {g10v1 qc=00 #3..65}");
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x01\x00\x00\x08\x5E\x01",9,
                                    "[1] (fir,fin) WRITE {g10v1 qc=00 #0..8: 0 1 1 1 1 0 1 0 1}");
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x01\x17\x00",6, "PARAM_ERROR on [1] (fir,fin) WRITE");
        // XXX is qc=17 (index prefixes) valid for bit-packed variation? which encoding is correct?
        //     e.g: "[1] (fir,fin) WRITE {g10v1 qc=17 #1:0,#4:1,#8:1}" "\x01\x02\x0A\x01\x17\x03...
        //     little-on-little
        //       00000001 0000100|0 001000|10 00000|100
        //       ...\x01\x08\x22\x04"
        //     big-on-little
        //       10000000 0100000|0 010000|10 00000|100
        //       ...\x80\x40\x42\x04"
        //     little-on-big
        //       10000000 0|0010000 01|000100 001|00000
        //       ...\x80\x10\x24\x20"
        //     big-on-big
        //       00000001 0|0000010 01|000010 001|00000
        //       ...\x01\x02\x42\x20"
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x01\x00\x00\x08\x5E\x02",9, "PARAM_ERROR on [1] (fir,fin) WRITE");
        // (an unused bit after the packed objects is not zero)
    check_parse(dnp3_p_app_response, "\xC1\x81\x00\x00\x0A\x01\x00\x00\x08\x5E\x01",11,
                                     "[1] (fir,fin) RESPONSE {g10v1 qc=00 #0..8: 0 1 1 1 1 0 1 0 1}");
    check_parse(dnp3_p_app_response, "\xC1\x81\x00\x00\x0A\x01\x00\x00\x08\x5E\x02",11,
                                     "PARAM_ERROR on [1] (fir,fin) RESPONSE");
        // (an unused bit after the packed objects is not zero)

    // v2 (flags)
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0A\x02\x17\x01\x03\x80",10,
                                     "[0] (fir,fin) RESPONSE {g10v2 qc=17 #3:1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0A\x02\x17\x01\x03\x83",10,
                                     "[0] (fir,fin) RESPONSE {g10v2 qc=17 #3:(online,restart)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0A\x02\x17\x01\x03\x89",10,
                                     "[0] (fir,fin) RESPONSE {g10v2 qc=17 #3:(online,remote_forced)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0A\x02\x17\x01\x03\x20",10,  // reserved bit set
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0A\x02\x17\x01\x03\x40",10,  // reserved bit set
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x02\x17\x01\x03\x80",9,
                                    "OBJ_UNKNOWN on [1] (fir,fin) WRITE");
}

static void test_obj_binoutev(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01\x0B\x00\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g11v0 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x0B\x01\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g11v1 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x0B\x02\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g11v2 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x0B\x03\x00\x03\x08",7,
                                    "OBJ_UNKNOWN on [0] (fir,fin) READ");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0B\x01\x17\x01\x03\x80",10,
                                     "[0] (fir,fin) RESPONSE {g11v1 qc=17 #3:1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0B\x01\x17\x01\x03\x83",10,
                                     "[0] (fir,fin) RESPONSE {g11v1 qc=17 #3:(online,restart)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0B\x01\x17\x01\x03\xC3",10,
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0B\x01\x17\x01\x03\xE3",10,
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0B\x02\x17\x01\x03\x80\x00\x00\x00\x00\x00\x00",16,
                                     "[0] (fir,fin) RESPONSE {g11v2 qc=17 #3:1@0s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0B\x02\x17\x01\x03\x01\x00\x00\x00\x00\x00\x80",16,
                                     "[0] (fir,fin) RESPONSE {g11v2 qc=17 #3:(online)0@140737488355.328s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0B\x02\x17\x01\x03\x82\xA0\xFC\x7D\x7A\x4B\x01",16,
                                     "[0] (fir,fin) RESPONSE {g11v2 qc=17 #3:(restart)1@1423689252s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0B\x02\x17\x01\x03\xC2\x00\x00\x00\x00\x00\x00",16,
                                     "PARAM_ERROR on [0] (fir,fin) RESPONSE");
}

static void test_obj_binoutcmd(void)
{
    check_parse(dnp3_p_app_response, "\x03\x81\x00\x00\x0C\x01\x00\x07\x07\x61\x03\x0A\x00\x00\x00\x2C\x01\x00\x00\x00",20,
                                     "[3] RESPONSE {g12v1 qc=00 #7..7: (CLOSE PULSE_ON clear 3x on=10ms off=300ms)}");
    check_parse(dnp3_p_app_response, "\x03\x81\x00\x00\x0C\x01\x00\x07\x07\x81\x03\x0A\x00\x00\x00\x2C\x01\x00\x00\x7F",20,
                                     "[3] RESPONSE {g12v1 qc=00 #7..7: (TRIP PULSE_ON 3x on=10ms off=300ms status=127)}");
    check_parse(dnp3_p_app_response, "\x03\x81\x00\x00\x0C\x02\x07\x01\x81\x03\x0A\x00\x00\x00\x2C\x01\x00\x00\x7F"
                                                     "\x0C\x03\x00\x01\x05",24,
                                     "[3] RESPONSE {g12v2 qc=07 (TRIP PULSE_ON 3x on=10ms off=300ms status=127)}"
                                                 " {g12v3 qc=00 #1..5}");
}

static void test_obj_binoutcmdev(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01\x0D\x00\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g13v0 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x0D\x01\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g13v1 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x0D\x02\x00\x03\x08",7,
                                    "[0] (fir,fin) READ {g13v2 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x0D\x03\x00\x03\x08",7,
                                    "OBJ_UNKNOWN on [0] (fir,fin) READ");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0D\x01\x17\x01\x03\xFF",10,
                                     "[0] (fir,fin) RESPONSE {g13v1 qc=17 #3:(status=127)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0D\x01\x17\x01\x03\x00",10,
                                     "[0] (fir,fin) RESPONSE {g13v1 qc=17 #3:0}");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0D\x02\x17\x01\x03\x80\x00\x00\x00\x00\x00\x00",16,
                                     "[0] (fir,fin) RESPONSE {g13v2 qc=17 #3:1@0s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0D\x02\x17\x01\x03\x80\x00\x00\x00\x00\x00\x80",16,
                                     "[0] (fir,fin) RESPONSE {g13v2 qc=17 #3:1@140737488355.328s}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0D\x02\x17\x01\x03\x80\xA0\xFC\x7D\x7A\x4B\x01",16,
                                     "[0] (fir,fin) RESPONSE {g13v2 qc=17 #3:1@1423689252s}");
}

static void test_obj_ctr(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x14\x01\x17\x01\x01\x41\x12\x34\x56\x78",14,
                                     "[0] RESPONSE {g20v1 qc=17 #1:(online,discontinuity)2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x14\x02\x17\x01\x01\x20\x12\x34",12,
                                     "[0] RESPONSE {g20v2 qc=17 #1:13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x14\x05\x17\x01\x01\x12\x34\x56\x78",13,
                                     "[0] RESPONSE {g20v5 qc=17 #1:2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x14\x06\x17\x01\x01\x12\x34",11,
                                     "[0] RESPONSE {g20v6 qc=17 #1:13330}");
}

static void test_obj_frozenctr(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x15\x01\x17\x01\x01\x41\x12\x34\x56\x78",14,
                                     "[0] RESPONSE {g21v1 qc=17 #1:(online,discontinuity)2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x15\x02\x17\x01\x01\x20\x12\x34",12,
                                     "[0] RESPONSE {g21v2 qc=17 #1:13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x15\x05\x17\x01\x01\x41\x12\x34\x56\x78\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g21v5 qc=17 #1:(online,discontinuity)2018915346@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x15\x06\x17\x01\x01\x20\x12\x34\xA0\xFC\x7D\x7A\x4B\x01",18,
                                     "[0] RESPONSE {g21v6 qc=17 #1:13330@1423689252s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x15\x09\x17\x01\x01\x12\x34\x56\x78",13,
                                     "[0] RESPONSE {g21v9 qc=17 #1:2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x15\x0A\x17\x01\x01\x12\x34",11,
                                     "[0] RESPONSE {g21v10 qc=17 #1:13330}");
}

static void test_obj_ctrev(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x16\x01\x17\x01\x01\x41\x12\x34\x56\x78",14,
                                     "[0] RESPONSE {g22v1 qc=17 #1:(online,discontinuity)2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x16\x02\x17\x01\x01\x20\x12\x34",12,
                                     "[0] RESPONSE {g22v2 qc=17 #1:13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x16\x05\x17\x01\x01\x41\x12\x34\x56\x78\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g22v5 qc=17 #1:(online,discontinuity)2018915346@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x16\x06\x17\x01\x01\x20\x12\x34\xA0\xFC\x7D\x7A\x4B\x01",18,
                                     "[0] RESPONSE {g22v6 qc=17 #1:13330@1423689252s}");
}

static void test_obj_frozenctrev(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x17\x01\x17\x01\x01\x41\x12\x34\x56\x78",14,
                                     "[0] RESPONSE {g23v1 qc=17 #1:(online,discontinuity)2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x17\x02\x17\x01\x01\x20\x12\x34",12,
                                     "[0] RESPONSE {g23v2 qc=17 #1:13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x17\x05\x17\x01\x01\x41\x12\x34\x56\x78\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g23v5 qc=17 #1:(online,discontinuity)2018915346@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x17\x06\x17\x01\x01\x20\x12\x34\xA0\xFC\x7D\x7A\x4B\x01",18,
                                     "[0] RESPONSE {g23v6 qc=17 #1:13330@1423689252s}");
}

static void test_obj_anain(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1E\x01\x17\x01\x01\x21\x12\x34\x56\x78",14,
                                     "[0] RESPONSE {g30v1 qc=17 #1:(online,over_range)2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1E\x02\x17\x01\x01\x40\x12\x34",12,
                                     "[0] RESPONSE {g30v2 qc=17 #1:(reference_err)13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1E\x03\x17\x01\x01\x12\x34\x56\x78",13,
                                     "[0] RESPONSE {g30v3 qc=17 #1:2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1E\x04\x17\x01\x01\x12\x34",11,
                                     "[0] RESPONSE {g30v4 qc=17 #1:13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1E\x05\x17\x01\x01\x21\x00\x00\x80\xBF",14,
                                     "[0] RESPONSE {g30v5 qc=17 #1:(online,over_range)-1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1E\x06\x17\x01\x01\x40\x00\x00\x00\x00\x00\x00\xF0\x3F",18,
                                     "[0] RESPONSE {g30v6 qc=17 #1:(reference_err)1.0}");
}

static void test_obj_frozenanain(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1F\x01\x17\x01\x01\x21\x12\x34\x56\x78",14,
                                     "[0] RESPONSE {g31v1 qc=17 #1:(online,over_range)2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1F\x02\x17\x01\x01\x40\x12\x34",12,
                                     "[0] RESPONSE {g31v2 qc=17 #1:(reference_err)13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1F\x03\x17\x01\x01\x21\x12\x34\x56\x78\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g31v3 qc=17 #1:(online,over_range)2018915346@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1F\x04\x17\x01\x01\x40\x12\x34\x00\x00\x00\x00\x00\x00",18,
                                     "[0] RESPONSE {g31v4 qc=17 #1:(reference_err)13330@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1F\x05\x17\x01\x01\x12\x34\x56\x78",13,
                                     "[0] RESPONSE {g31v5 qc=17 #1:2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1F\x06\x17\x01\x01\x12\x34",11,
                                     "[0] RESPONSE {g31v6 qc=17 #1:13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1F\x07\x17\x01\x01\x21\x00\x00\x80\xBF",14,
                                     "[0] RESPONSE {g31v7 qc=17 #1:(online,over_range)-1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x1F\x08\x17\x01\x01\x40\x00\x00\x00\x00\x00\x00\xF0\x3F",18,
                                     "[0] RESPONSE {g31v8 qc=17 #1:(reference_err)1.0}");
}

static void test_obj_anainev(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x20\x01\x17\x01\x01\x21\x12\x34\x56\x78",14,
                                     "[0] RESPONSE {g32v1 qc=17 #1:(online,over_range)2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x20\x02\x17\x01\x01\x40\x12\x34",12,
                                     "[0] RESPONSE {g32v2 qc=17 #1:(reference_err)13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x20\x03\x17\x01\x01\x21\x12\x34\x56\x78\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g32v3 qc=17 #1:(online,over_range)2018915346@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x20\x04\x17\x01\x01\x40\x12\x34\x00\x00\x00\x00\x00\x00",18,
                                     "[0] RESPONSE {g32v4 qc=17 #1:(reference_err)13330@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x20\x05\x17\x01\x01\x21\x00\x00\x80\xBF",14,
                                     "[0] RESPONSE {g32v5 qc=17 #1:(online,over_range)-1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x20\x06\x17\x01\x01\x40\x00\x00\x00\x00\x00\x00\xF0\x3F",18,
                                     "[0] RESPONSE {g32v6 qc=17 #1:(reference_err)1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x20\x07\x17\x01\x01\x21\x00\x00\x80\xBF\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g32v7 qc=17 #1:(online,over_range)-1.0@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x20\x08\x17\x01\x01\x40\x00\x00\x00\x00\x00\x00\xF0\x3F\x00\x00\x00\x00\x00\x00",24,
                                     "[0] RESPONSE {g32v8 qc=17 #1:(reference_err)1.0@0s}");
}

static void test_obj_frozenanainev(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x21\x01\x17\x01\x01\x21\x12\x34\x56\x78",14,
                                     "[0] RESPONSE {g33v1 qc=17 #1:(online,over_range)2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x21\x02\x17\x01\x01\x40\x12\x34",12,
                                     "[0] RESPONSE {g33v2 qc=17 #1:(reference_err)13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x21\x03\x17\x01\x01\x21\x12\x34\x56\x78\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g33v3 qc=17 #1:(online,over_range)2018915346@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x21\x04\x17\x01\x01\x40\x12\x34\x00\x00\x00\x00\x00\x00",18,
                                     "[0] RESPONSE {g33v4 qc=17 #1:(reference_err)13330@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x21\x05\x17\x01\x01\x21\x00\x00\x80\xBF",14,
                                     "[0] RESPONSE {g33v5 qc=17 #1:(online,over_range)-1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x21\x06\x17\x01\x01\x40\x00\x00\x00\x00\x00\x00\xF0\x3F",18,
                                     "[0] RESPONSE {g33v6 qc=17 #1:(reference_err)1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x21\x07\x17\x01\x01\x21\x00\x00\x80\xBF\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g33v7 qc=17 #1:(online,over_range)-1.0@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x21\x08\x17\x01\x01\x40\x00\x00\x00\x00\x00\x00\xF0\x3F\x00\x00\x00\x00\x00\x00",24,
                                     "[0] RESPONSE {g33v8 qc=17 #1:(reference_err)1.0@0s}");
}

static void test_obj_anaindeadband(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x22\x01\x17\x01\x01\x12\x34",11,
                                     "[0] RESPONSE {g34v1 qc=17 #1:13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x22\x02\x17\x01\x01\x12\x34\x56\x78",13,
                                     "[0] RESPONSE {g34v2 qc=17 #1:2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x22\x03\x17\x01\x01\x00\x00\x80\xBF",13,
                                     "PARAM_ERROR on [0] RESPONSE");    // negative deadband value
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x22\x03\x17\x01\x01\x00\x00\x80\x3F",13,
                                     "[0] RESPONSE {g34v3 qc=17 #1:1.0}");

    check_parse(dnp3_p_app_request, "\xC0\x02\x22\x01\x17\x01\x01\x12\x34",9,
                                    "[0] (fir,fin) WRITE {g34v1 qc=17 #1:13330}");
    check_parse(dnp3_p_app_request, "\xC0\x02\x22\x02\x17\x01\x01\x12\x34\x56\x78",11,
                                    "[0] (fir,fin) WRITE {g34v2 qc=17 #1:2018915346}");
    check_parse(dnp3_p_app_request, "\xC0\x02\x22\x03\x17\x01\x01\x00\x00\x80\xBF",11,
                                    "PARAM_ERROR on [0] (fir,fin) WRITE");    // negative deadband value
    check_parse(dnp3_p_app_request, "\xC0\x02\x22\x03\x17\x01\x01\x00\x00\x80\x3F",11,
                                    "[0] (fir,fin) WRITE {g34v3 qc=17 #1:1.0}");
}

static void test_obj_anaoutstatus(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x28\x01\x17\x01\x01\x21\x12\x34\x56\x78",14,
                                     "[0] RESPONSE {g40v1 qc=17 #1:(online,over_range)2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x28\x02\x17\x01\x01\x40\x12\x34",12,
                                     "[0] RESPONSE {g40v2 qc=17 #1:(reference_err)13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x28\x03\x17\x01\x01\x21\x00\x00\x80\xBF",14,
                                     "[0] RESPONSE {g40v3 qc=17 #1:(online,over_range)-1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x28\x04\x17\x01\x01\x40\x00\x00\x00\x00\x00\x00\xF0\x3F",18,
                                     "[0] RESPONSE {g40v4 qc=17 #1:(reference_err)1.0}");
}

static void test_obj_anaout(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x29\x01\x17\x01\x01\x12\x34\x56\x78\x00",14,
                                     "[0] RESPONSE {g41v1 qc=17 #1:2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x29\x02\x17\x01\x01\x12\x34\x7F",12,
                                     "[0] RESPONSE {g41v2 qc=17 #1:(status=127)13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x29\x03\x17\x01\x01\x00\x00\x80\xBF\x08",14,
                                     "[0] RESPONSE {g41v3 qc=17 #1:(status=8)-1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x29\x04\x17\x01\x01\x00\x00\x00\x00\x00\x00\xF0\x3F\x1F",18,
                                     "[0] RESPONSE {g41v4 qc=17 #1:(status=31)1.0}");
}

static void test_obj_anaoutev(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2A\x01\x17\x01\x01\x21\x12\x34\x56\x78",14,
                                     "[0] RESPONSE {g42v1 qc=17 #1:(online,over_range)2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2A\x02\x17\x01\x01\x40\x12\x34",12,
                                     "[0] RESPONSE {g42v2 qc=17 #1:(reference_err)13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2A\x03\x17\x01\x01\x21\x12\x34\x56\x78\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g42v3 qc=17 #1:(online,over_range)2018915346@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2A\x04\x17\x01\x01\x40\x12\x34\x00\x00\x00\x00\x00\x00",18,
                                     "[0] RESPONSE {g42v4 qc=17 #1:(reference_err)13330@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2A\x05\x17\x01\x01\x21\x00\x00\x80\xBF",14,
                                     "[0] RESPONSE {g42v5 qc=17 #1:(online,over_range)-1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2A\x06\x17\x01\x01\x40\x00\x00\x00\x00\x00\x00\xF0\x3F",18,
                                     "[0] RESPONSE {g42v6 qc=17 #1:(reference_err)1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2A\x07\x17\x01\x01\x21\x00\x00\x80\xBF\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g42v7 qc=17 #1:(online,over_range)-1.0@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2A\x08\x17\x01\x01\x40\x00\x00\x00\x00\x00\x00\xF0\x3F\x00\x00\x00\x00\x00\x00",24,
                                     "[0] RESPONSE {g42v8 qc=17 #1:(reference_err)1.0@0s}");
}

static void test_obj_anaoutcmdev(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2B\x01\x17\x01\x01\x21\x12\x34\x56\x78",14,
                                     "[0] RESPONSE {g43v1 qc=17 #1:(status=33)2018915346}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2B\x02\x17\x01\x01\x40\x12\x34",12,
                                     "[0] RESPONSE {g43v2 qc=17 #1:(status=64)13330}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2B\x03\x17\x01\x01\x21\x12\x34\x56\x78\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g43v3 qc=17 #1:(status=33)2018915346@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2B\x04\x17\x01\x01\x40\x12\x34\x00\x00\x00\x00\x00\x00",18,
                                     "[0] RESPONSE {g43v4 qc=17 #1:(status=64)13330@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2B\x05\x17\x01\x01\x21\x00\x00\x80\xBF",14,
                                     "[0] RESPONSE {g43v5 qc=17 #1:(status=33)-1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2B\x06\x17\x01\x01\x40\x00\x00\x00\x00\x00\x00\xF0\x3F",18,
                                     "[0] RESPONSE {g43v6 qc=17 #1:(status=64)1.0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2B\x07\x17\x01\x01\x21\x00\x00\x80\xBF\x00\x00\x00\x00\x00\x00",20,
                                     "[0] RESPONSE {g43v7 qc=17 #1:(status=33)-1.0@0s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x2B\x08\x17\x01\x01\x40\x00\x00\x00\x00\x00\x00\xF0\x3F\x00\x00\x00\x00\x00\x00",24,
                                     "[0] RESPONSE {g43v8 qc=17 #1:(status=64)1.0@0s}");
}

static void test_obj_time(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x32\x01\x07\x01\x00\x04\x00\x00\x00\x00",14,
                                     "[0] RESPONSE {g50v1 qc=07 @1.024s}");
    check_parse(dnp3_p_app_request,  "\xC0\x0B\x32\x02\x07\x01\x00\x04\x00\x00\x00\x00\x3C\x00\x00\x00\x14\x00\x06",19,
                                     "[0] (fir,fin) FREEZE_AT_TIME {g50v2 qc=07 @1.024s+0.060s} {g20v0 qc=06}");
    check_parse(dnp3_p_app_request,  "\xC0\x02\x32\x03\x07\x01\x00\x04\x00\x00\x00\x00",12,
                                     "[0] (fir,fin) WRITE {g50v3 qc=07 @1.024s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x32\x04\x17\x01\x01\x00\x04\x00\x00\x00\x00\x0A\x00\x00\x00\x05",20,
                                     "[0] RESPONSE {g50v4 qc=17 #1:@1.024s+10d}");
}

static void test_obj_cto(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x33\x01\x07\x01\x00\x04\x00\x00\x00\x00",14,
                                     "[0] RESPONSE {g51v1 qc=07 @1.024s}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x33\x02\x07\x01\x00\x04\x00\x00\x00\x00",14,
                                     "[0] RESPONSE {g51v2 qc=07 (unsynchronized)@1.024s}");
}

static void test_obj_delay(void)
{
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x34\x01\x07\x01\x10\x00",10,
                                     "[0] RESPONSE {g52v1 qc=07 16000ms}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x34\x02\x07\x01\x00\x04",10,
                                     "[0] RESPONSE {g52v2 qc=07 1024ms}");
}

static void test_obj_class(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01\x3C\x01\x06",5,         "[0] (fir,fin) READ {g60v1 qc=06}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x3C\x02\x06",5,         "[0] (fir,fin) READ {g60v2 qc=06}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x3C\x03\x07\x23",6,     "[0] (fir,fin) READ {g60v3 qc=07 range=35}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x3C\x04\x08\x04\x01",7, "[0] (fir,fin) READ {g60v4 qc=08 range=260}");
}

static void test_obj_iin(void)
{
    check_parse(dnp3_p_app_request,  "\xC0\x01\x50\x01\x17\x03\x10\x08\x01",9,
                                     "[0] (fir,fin) READ {g80v1 qc=17 #16 #8 #1}");
    check_parse(dnp3_p_app_request,  "\xC0\x02\x50\x01\x00\x07\x07\x00",8,
                                     "[0] (fir,fin) WRITE {g80v1 qc=00 #7..7: 0}");
    check_parse(dnp3_p_app_response, "\x00\x81\x00\x00\x50\x01\x00\x00\x02\x06",10,
                                     "[0] RESPONSE {g80v1 qc=00 #0..2: 0 1 1}");
}

static void test_link_raw(void)
{
    check_parse(dnp3_p_link_frame, "\x05\x64\x05\xF2\x01\x00\xEF\xFF\xBF\xB5",10,
                                   "primary frame from master 65519 to 1: (fcb=1) TEST_LINK_STATES");
    check_parse(dnp3_p_link_frame, "\x05\x64\x05\xF2\x01\x00\xF0\xFF\x62\x82",10,
                                   "primary frame from master 65520 to 1: (fcb=1) TEST_LINK_STATES");
    check_parse_fail(dnp3_p_link_frame, "\x05\x64\x05\xF2\x01\x00\xEF\xFF\xBF\xB4",10); // crc error
    check_parse(dnp3_p_link_frame, "\x05\x64\x09\xF3\x01\x00\xEF\xFF\x0B\x41\x01\x02\x03\x05\xB4\x67\x58",17,
                                   "primary frame from master 65519 to 1: (fcb=1) CONFIRMED_USER_DATA: <corrupt>");
    check_parse(dnp3_p_link_frame, "\x05\x64\x09\xF3\x01\x00\xEF\xFF\x0B\x41\x01\x02\x03\x04\xB4\x67\x58",17,
                                   "primary frame from master 65519 to 1: (fcb=1) CONFIRMED_USER_DATA: 01 02 03 04");
    check_parse(dnp3_p_link_frame, "\x05\x64\x26\xF3\x01\x00\xEF\xFF\x6B\xF4"
                                   "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\xEC\x10"
                                   "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x27\x03"
                                   "\x20\x50\xD6",49,
                                   "primary frame from master 65519 to 1: (fcb=1) CONFIRMED_USER_DATA:"
                                   " 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F"
                                   " 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F"
                                   " 20");

    check_parse(dnp3_p_link_frame, "\x05\x64\x05\xD2\x01\x00\xEF\xFF\xE2\xAD",10,
                                   "primary frame from master 65519 to 1: (fcb=0) TEST_LINK_STATES");
    check_parse(dnp3_p_link_frame, "\x05\x64\x05\xC2\x01\x00\xEF\xFF\x70\x07",10,
                                   "primary frame from master 65519 to 1: TEST_LINK_STATES");
    check_parse(dnp3_p_link_frame, "\x05\x64\x05\x90\x01\x00\xEF\xFF\x54\xDB",10,
                                   "secondary frame from master 65519 to 1: (dfc) ACK");
}

static void do_check_frame(bool valid, const uint8_t *input, size_t len, const char *result, int LINE)
{
    HParseResult *res = h_parse(dnp3_p_link_frame, input, len);
    if (!res) {
      g_test_message("Parse failed on line %d, while expecting %s", LINE, result);
      g_test_fail();
    } else {
      char *cres = format(res->ast);
      check_string(cres, == , result);
      free(cres);
      if(dnp3_link_validate_frame(res->ast->user) != valid) {
        if(valid)
          g_test_message("Frame invalid on line %d", LINE);
        else
          g_test_message("Frame should not be valid on line %d, but was", LINE);
        g_test_fail();
      }
      h_parse_result_free(res);
    }
}

#define check_frame_valid(input, inp_len, result) do { \
    do_check_frame(true, (const uint8_t*) input, inp_len, result, __LINE__); \
} while(0)

#define check_frame_invalid(input, inp_len, result) do { \
    do_check_frame(false, (const uint8_t*) input, inp_len, result, __LINE__); \
} while(0)

static void test_link_valid(void)
{
    check_frame_valid("\x05\x64\x05\xF2\x01\x00\xEF\xFF\xBF\xB5",10,
                      "primary frame from master 65519 to 1: (fcb=1) TEST_LINK_STATES");
    check_frame_invalid("\x05\x64\x05\xF2\x01\x00\xF0\xFF\x62\x82",10,  // inv. source addr.
                        "primary frame from master 65520 to 1: (fcb=1) TEST_LINK_STATES");
    check_frame_invalid("\x05\x64\x05\xE2\xFB\xFF\xEF\xFF\x5B\xE2",10,
                        "primary frame from master 65519 to 65531: TEST_LINK_STATES");
    check_frame_invalid("\x05\x64\x06\xF2\x01\x00\xEF\xFF\xEF\x26\x01\xA1\xC9",13,  // extra payload
                        "primary frame from master 65519 to 1: (fcb=1) TEST_LINK_STATES: 01");
    check_frame_invalid("\x05\x64\x05\xE1\x01\x00\xEF\xFF\x27\x7A",10,
                        "primary frame from master 65519 to 1: function 1 (obsolete)");
    check_frame_invalid("\x05\x64\x05\xEF\x01\x00\xEF\xFF\x7A\xE5",10,
                        "primary frame from master 65519 to 1: function 15 (reserved)");
    check_frame_invalid("\x05\x64\x05\xB3\x01\x00\xEF\xFF\x03\xA6",10,
                        "secondary frame from master 65519 to 1: (fcb=1,dfc) function 3 (reserved)");
    check_frame_valid("\x05\x64\x05\xA1\x01\x00\xEF\xFF\x9D\x4A",10,
                      "secondary frame from master 65519 to 1: (fcb=1) NACK");
    check_frame_invalid("\x05\x64\x05\xE2\x01\x00\xEF\xFF\x2D\x1F",10,  // fcv=0
                        "primary frame from master 65519 to 1: TEST_LINK_STATES");
    check_frame_invalid("\x05\x64\x07\xF4\x01\x00\xEF\xFF\x1C\x59\x00\x00\xFF\xFF",14, // fcv=1
                        "primary frame from master 65519 to 1: (fcb=1) UNCONFIRMED_USER_DATA: 00 00");
    check_frame_invalid("\x05\x64\x05\xD2\xFD\xFF\xEF\xFF\x16\x44",10,  // broadcast not user-data
                        "primary frame from master 65519 to 65533: (fcb=0) TEST_LINK_STATES");
    check_frame_valid("\x05\x64\x07\xE4\xFD\xFF\xEF\xFF\x7A\x1A\x00\x00\xFF\xFF",14,
                      "primary frame from master 65519 to 65533: UNCONFIRMED_USER_DATA: 00 00");

    check_frame_valid("\x05\x64\x09\xF3\x01\x00\xEF\xFF\x0B\x41\x01\x02\x03\x04\xB4\x67\x58",17,
                      "primary frame from master 65519 to 1: (fcb=1) CONFIRMED_USER_DATA: 01 02 03 04");
    check_frame_invalid("\x05\x64\x05\xF3\x01\x00\xEF\xFF\xB9\x96",10,  // empty payload
                        "primary frame from master 65519 to 1: (fcb=1) CONFIRMED_USER_DATA");
    check_frame_invalid("\x05\x64\x05\xF4\x01\x00\xEF\xFF\xAB\x7F",10,  // empty payload
                        "primary frame from master 65519 to 1: (fcb=1) UNCONFIRMED_USER_DATA");
    check_frame_invalid("\x05\x64\x09\xF3\x01\x00\xEF\xFF\x0B\x41\x01\x02\x03\x05\xB4\x67\x58",17,  // corrupt payload
                        "primary frame from master 65519 to 1: (fcb=1) CONFIRMED_USER_DATA: <corrupt>");
}

static void do_check_frame_skip(const uint8_t *input, size_t len, size_t skip, int LINE)
{
    HParseResult *res = h_parse(dnp3_p_link_frame, input, len);
    if (!res) {
      g_test_message("Parse failed on line %d", LINE);
      g_test_fail();
    } else {
      check_cmp_size(res->bit_length%8, ==, 0);
      check_cmp_size(res->bit_length/8, ==, skip);
      h_parse_result_free(res);
    }
}

#define check_frame_skip(input, len, skip) \
    do_check_frame_skip((const uint8_t *)(input), len, skip, __LINE__)

static void test_link_skip(void)
{
    check_frame_skip("\x05\x64\x05\xF2\x01\x00\xEF\xFF\xBF\xB5",10, 10); // valid
    check_frame_skip("\x05\x64\x05\xF2\x01\x00\xEF\xFF\xBF\xB5\x00\x00",12, 10); // valid
    check_frame_skip("\x05\x64\x05\xF2\x01\x00\xF0\xFF\x62\x82\x00\x00",12, 10); // inv. source addr.
    check_frame_skip("\x05\x64\x03\xF2\x05\x64\x05\x64\xA9\x8E\x00\x00",12, 10); // inv. length (3)
    check_frame_skip("\x05\x64\x09\xF3\x01\x00\xEF\xFF\x0B\x41"
                     "\x01\x02\x03\x04\xFF\xFF",16, 16);        // wrong CRC
    check_frame_skip("\x05\x64\x29\xF3\x01\x00\xEF\xFF\x89\xB0"
                     "___4___8__12__16\x34\x90"
                     "___4___8__12__16\xFF\xFF"                 // wrong CRC
                     "\x01\x02\x03\x04\xB4\x67",52, 52);
}

static void test_transport(void)
{
    check_parse(dnp3_p_transport_segment, "\x4A\x01\x02\x03\x04\x05\x06",7,
                                          "(fir) segment 10: 01 02 03 04 05 06");
    check_parse(dnp3_p_transport_segment, "\x9A",1, "(fin) segment 26:");
    check_parse_fail(dnp3_p_transport_segment, "",0);
}



/// ...

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);
    dnp3_init();

    // tests of crashes found via AFL
    g_test_add_func("/app/crash/1", test_crash1);

    // specific tests for common dnp3 vulnerabilties
    g_test_add_func("/app/vuln/range_overflow", test_range_overflow);
    g_test_add_func("/app/vuln/mult_overflow", test_mult_overflow);
    g_test_add_func("/app/vuln/count_of_zero", test_count_of_zero);
    g_test_add_func("/app/vuln/large_packed_binary", test_large_packed_binary);

    g_test_add_func("/app/fragment", test_app_fragment);
    g_test_add_func("/app/req/fail", test_req_fail);
    g_test_add_func("/app/req/ac", test_req_ac);
    g_test_add_func("/app/req/ohdr", test_req_ohdr);
    g_test_add_func("/app/req/confirm", test_req_confirm);
    g_test_add_func("/app/req/read", test_req_read);
    g_test_add_func("/app/req/write", test_req_write);
    g_test_add_func("/app/req/select", test_req_select);
    g_test_add_func("/app/req/select_float", test_req_select_float);
    g_test_add_func("/app/req/operate", test_req_operate);
    g_test_add_func("/app/req/direct_operate", test_req_direct_operate);
    g_test_add_func("/app/req/direct_operate_nr", test_req_direct_operate_nr);
    g_test_add_func("/app/req/freeze", test_req_freeze);
    g_test_add_func("/app/req/freeze_clear", test_req_freeze_clear);
    g_test_add_func("/app/req/freeze_at_time", test_req_freeze_at_time);
    g_test_add_func("/app/req/restart", test_req_restart);
    g_test_add_func("/app/req/initialize_data", test_req_initialize_data);
    g_test_add_func("/app/req/application", test_req_application);
    g_test_add_func("/app/req/enable_unsolicited", test_req_enable_unsolicited);
    g_test_add_func("/app/req/disable_unsolicited", test_req_disable_unsolicited);
    g_test_add_func("/app/req/assign_class", test_req_assign_class);
    g_test_add_func("/app/req/delay_measure", test_req_delay_measure);
    g_test_add_func("/app/req/record_current_time", test_req_record_current_time);
    g_test_add_func("/app/rsp/fail", test_rsp_fail);
    g_test_add_func("/app/rsp/ac", test_rsp_ac);
    g_test_add_func("/app/rsp/iin", test_rsp_iin);
    g_test_add_func("/app/rsp/null", test_rsp_null);
    g_test_add_func("/app/rsp/unsolicited", test_rsp_unsolicited);
    g_test_add_func("/app/obj/binin", test_obj_binin);
    g_test_add_func("/app/obj/bininev", test_obj_bininev);
    g_test_add_func("/app/obj/dblbitin", test_obj_dblbitin);
    g_test_add_func("/app/obj/dblbitinev", test_obj_dblbitinev);
    g_test_add_func("/app/obj/binout", test_obj_binout);
    g_test_add_func("/app/obj/binoutev", test_obj_binoutev);
    g_test_add_func("/app/obj/binoutcmd", test_obj_binoutcmd);
    g_test_add_func("/app/obj/binoutcmdev", test_obj_binoutcmdev);
    g_test_add_func("/app/obj/ctr", test_obj_ctr);
    g_test_add_func("/app/obj/ctrev", test_obj_ctrev);
    g_test_add_func("/app/obj/frozenctr", test_obj_frozenctr);
    g_test_add_func("/app/obj/frozenctrev", test_obj_frozenctrev);
    g_test_add_func("/app/obj/anain", test_obj_anain);
    g_test_add_func("/app/obj/frozenanain", test_obj_frozenanain);
    g_test_add_func("/app/obj/anainev", test_obj_anainev);
    g_test_add_func("/app/obj/frozenanainev", test_obj_frozenanainev);
    g_test_add_func("/app/obj/anaindeadband", test_obj_anaindeadband);
    g_test_add_func("/app/obj/anaoutstatus", test_obj_anaoutstatus);
    g_test_add_func("/app/obj/anaout", test_obj_anaout);
    g_test_add_func("/app/obj/anaoutev", test_obj_anaoutev);
    g_test_add_func("/app/obj/anaoutcmdev", test_obj_anaoutcmdev);
    g_test_add_func("/app/obj/time", test_obj_time);
    g_test_add_func("/app/obj/cto", test_obj_cto);
    g_test_add_func("/app/obj/delay", test_obj_delay);
    g_test_add_func("/app/obj/class", test_obj_class);
    g_test_add_func("/app/obj/iin", test_obj_iin);
    g_test_add_func("/transport", test_transport);
    g_test_add_func("/link/raw", test_link_raw);
    g_test_add_func("/link/valid", test_link_valid);
    g_test_add_func("/link/skip", test_link_skip);

    g_test_run();
}
