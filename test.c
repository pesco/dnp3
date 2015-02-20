#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>   // PRIu64
#include <glib.h>
#include <dnp3.h>
#include "src/hammer.h" // H_ISERR


/// test macros (lifted/adapted from hammer's test suite) ///

// a function to format any known parsed token type
static char *format(const HParsedToken *p)
{
    if(!p)
        return h_write_result_unamb(p);

    switch(p->token_type) {
    case TT_DNP3_Fragment:  return dnp3_format_fragment(p->user);
    }

    if(H_ISERR(p->token_type)) {
        char *name = NULL;
        switch(p->token_type) {
        case ERR_FUNC_NOT_SUPP: name = g_strdup("FUNC_NOT_SUPP"); break;
        case ERR_OBJ_UNKNOWN:   name = g_strdup("OBJ_UNKNOWN"); break;
        case ERR_PARAM_ERROR:   name = g_strdup("PARAM_ERROR"); break;
        default:
            name = g_strdup_printf("ERROR %d", p->token_type-TT_ERR);
        }

        char *frag = NULL;
        if(p->user)
            frag = dnp3_format_fragment(p->user);
        else
            frag = g_strdup("-?-");

        return g_strdup_printf("%s on %s", name, frag);
    }

    return h_write_result_unamb(p);
}

#define check_numtype(fmt, typ, n1, op, n2) do {				\
    typ _n1 = (n1);							\
    typ _n2 = (n2);							\
    if (!(_n1 op _n2)) {						\
      g_test_message("Check failed on line %d: (%s): (" fmt " %s " fmt ")",	\
		     __LINE__,				\
		     #n1 " " #op " " #n2,				\
		     _n1, #op, _n2);					\
      g_test_fail();							\
    }									\
  } while(0)

#define check_cmp_uint64(n1, op, n2) check_numtype("%" PRIu64, uint64_t, n1, op, n2)

#define check_string(n1, op, n2) do {			\
    const char *_n1 = (n1);				\
    const char *_n2 = (n2);				\
    if (!(strcmp(_n1, _n2) op 0)) {			\
      g_test_message("Check failed on line %d: (%s) (%s %s %s)",	\
		     __LINE__,				\
		     #n1 " " #op " " #n2,		\
		     _n1, #op, _n2);			\
      g_test_fail();					\
    }							\
  } while(0)

#define check_parse_fail(parser, input, inp_len) do { \
    const HParseResult *result = h_parse(parser, (const uint8_t*)input, inp_len); \
    if (NULL != result) { \
      char* cres = format(result->ast); \
      g_test_message("Check failed on line %d: shouldn't have succeeded, but parsed %s", \
                     __LINE__, cres); \
      free(cres); \
      g_test_fail(); \
    } \
  } while(0)

#define check_parse(parser, input, inp_len, result) do { \
    HParseResult *res = h_parse(parser, (const uint8_t*)input, inp_len); \
    if (!res) { \
      g_test_message("Parse failed on line %d, while expecting %s", __LINE__, result); \
      g_test_fail(); \
    } else { \
      char* cres = format(res->ast); \
      check_string(cres, ==, result); \
      free(cres); \
      HArenaStats stats; \
      h_allocator_stats(res->arena, &stats); \
      g_test_message("Parse used %zd bytes, wasted %zd bytes. " \
                     "Inefficiency: %5f%%", \
		     stats.used, stats.wasted, \
		     stats.wasted * 100. / (stats.used+stats.wasted)); \
      h_delete_arena(res->arena); \
    } \
  } while(0)


/// test cases ///

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
        // XXX should a message with unexpected objects yield OBJ_UNKNOWN?
}

static void test_req_read(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01",2, "[0] (fir,fin) READ");  // XXX null READ valid?
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17\x03\x41\x43\x42",9,
                                    "[0] (fir,fin) READ {g1v0 qc=17 #65 #67 #66}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17\x00",6,   // XXX is 0 a valid count?
                                    "[0] (fir,fin) READ {g1v0 qc=17}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x00\x03\x41",7,
                                    "[0] (fir,fin) READ {g1v0 qc=00 #3..65}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x02\x03\x00\x03\x41",7,
                                    "[0] (fir,fin) READ {g2v3 qc=00 #3..65}");
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
    //check_parse(dnp3_p_app_request,  "\xC0\x81\x32\x02\x17\x01\x01\x00\x04\x00\x00\x00\x00\x3C\x00\x00\x00",17,
    // XXX                             "[0] (fir,fin) FREEZE {g50v2 qc=17 #1:@1.024s+60s}");
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



/// ...

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);
    dnp3_p_init();

    g_test_add_func("/app/req/fail", test_req_fail);
    g_test_add_func("/app/req/ac", test_req_ac);
    g_test_add_func("/app/req/ohdr", test_req_ohdr);
    g_test_add_func("/app/req/confirm", test_req_confirm);
    g_test_add_func("/app/req/read", test_req_read);
    g_test_add_func("/app/req/write", test_req_write);
    g_test_add_func("/app/req/select", test_req_select);
    g_test_add_func("/app/req/operate", test_req_operate);
    g_test_add_func("/app/req/direct_operate", test_req_direct_operate);
    g_test_add_func("/app/req/direct_operate_nr", test_req_direct_operate_nr);
    g_test_add_func("/app/req/freeze", test_req_freeze);
    g_test_add_func("/app/req/freeze_clear", test_req_freeze_clear);
    g_test_add_func("/app/req/freeze_at_time", test_req_freeze_at_time);
    g_test_add_func("/app/req/restart", test_req_restart);
    g_test_add_func("/app/rsp/fail", test_rsp_fail);
    g_test_add_func("/app/rsp/ac", test_rsp_ac);
    g_test_add_func("/app/rsp/iin", test_rsp_iin);
    g_test_add_func("/app/rsp/null", test_rsp_null);
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

    g_test_run();
}
