#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>   // PRIu64
#include <glib.h>
#include <dnp3.h>
#include "src/util.h"   // ISERR


/// test macros (lifted/adapted from hammer's test suite) ///

// a function to format any known parsed token type
static char *format(const HParsedToken *p)
{
    if(!p)
        return h_write_result_unamb(p);

    switch(p->token_type) {
    case TT_DNP3_Request:   return dnp3_format_request(p->user);
    case ERR_FUNC_NOT_SUPP: return g_strdup("FUNC_NOT_SUPP");
    case ERR_OBJ_UNKNOWN:   return g_strdup("OBJ_UNKNOWN");
    case ERR_PARAM_ERROR:   return g_strdup("PARAM_ERROR");
    }

    if(ISERR(p->token_type)) {
        char *s = malloc(5);
        if(!s) return NULL;
        snprintf(s, 5, "e%.02d", p->token_type);
        return s;
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
    check_parse_fail(dnp3_p_app_request, "\x00",1);
    check_parse(dnp3_p_app_request, "\xC0\x23",2, "FUNC_NOT_SUPP");
    check_parse(dnp3_p_app_request, "\xC0\x81",2, "FUNC_NOT_SUPP");
}

static void test_req_ac(void)
{
    check_parse(dnp3_p_app_request, "\xC2\x00",2, "[2] (fin,fir) CONFIRM");
    check_parse(dnp3_p_app_request, "\xC5\x00",2, "[5] (fin,fir) CONFIRM");
    check_parse(dnp3_p_app_request, "\xD3\x00",2, "[3] (fin,fir,uns) CONFIRM");
    check_parse_fail(dnp3_p_app_request, "\xE0\x23",2); // (con) ???
    check_parse_fail(dnp3_p_app_request, "\xE0\x81",2); // (con) RESPONSE
    check_parse_fail(dnp3_p_app_request, "\xD0\x23",2); // (uns) ???
    check_parse_fail(dnp3_p_app_request, "\xD0\x81",2); // (uns) RESPONSE
    check_parse_fail(dnp3_p_app_request, "\xD0\x01",2); // (uns) READ
    check_parse_fail(dnp3_p_app_request, "\x40\x00",2); // not (fin)
    check_parse_fail(dnp3_p_app_request, "\x80\x00",2); // not (fir)
    check_parse_fail(dnp3_p_app_request, "\xE0\x00",2); // (con)
    check_parse_fail(dnp3_p_app_request, "\xF0\x00",2); // (con,uns)
    check_parse(dnp3_p_app_request, "\xC0\x00",2, "[0] (fin,fir) CONFIRM");
}

static void test_req_ohdr(void)
{
    // truncated (otherwise valid) object header
    check_parse_fail(dnp3_p_app_request, "\xC0\x01\x01",3);
    check_parse_fail(dnp3_p_app_request, "\xC0\x01\x01\x00",4);
    check_parse_fail(dnp3_p_app_request, "\xC0\x01\x01\x00\x17",5);
    check_parse_fail(dnp3_p_app_request, "\xC0\x01\x01\x00\x17\x01",6);
    check_parse_fail(dnp3_p_app_request, "\xC0\x01\x01\x00\x7A",5);

    // truncated object header (invalid group)
    check_parse_fail(dnp3_p_app_request, "\xC0\x01\x05",3);
    check_parse_fail(dnp3_p_app_request, "\xC0\x01\x05\x00",4);
    check_parse_fail(dnp3_p_app_request, "\xC0\x01\x01\x00\x00\x03\x41\x58",8);
    check_parse_fail(dnp3_p_app_request, "\xC0\x01\x01\x00\x06\x05\x00",7);

    // truncated object header (invalid variation)
    check_parse_fail(dnp3_p_app_request, "\xC0\x01\x32\x00",4);

    // invalid group / variation (complete header)
    check_parse(dnp3_p_app_request, "\xC0\x01\x05\x00\x00\x03\x41",7, "OBJ_UNKNOWN");
    check_parse(dnp3_p_app_request, "\xC0\x01\x32\x00\x06",5, "OBJ_UNKNOWN");
}

static void test_req_confirm(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x00",2, "[0] (fin,fir) CONFIRM");
    check_parse(dnp3_p_app_request, "\xD0\x00",2, "[0] (fin,fir,uns) CONFIRM");
    check_parse_fail(dnp3_p_app_request, "\xC0\x00\x01\x00\x06",5);
}

static void test_req_read(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01",2, "[0] (fin,fir) READ");  // XXX null READ valid?
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17\x03\x41\x43\x42",9,
                                    "[0] (fin,fir) READ {g1v0 qc=17 #65 #67 #66}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17\x00",6,   // XXX is 0 a valid count?
                                    "[0] (fin,fir) READ {g1v0 qc=17}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x00\x03\x41",7,
                                    "[0] (fin,fir) READ {g1v0 qc=00 #3..65}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x02\x03\x00\x03\x41",7,
                                    "[0] (fin,fir) READ {g2v3 qc=00 #3..65}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x0A\x01\x00\x03\x41",7,
                                    "[0] (fin,fir) READ {g10v1 qc=00 #3..65}");
}

static void test_req_write(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x02",2, "[0] (fin,fir) WRITE"); // XXX null WRITE valid?
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x00\x00\x03\x06",7, "OBJ_UNKNOWN");
        // (variation 0 ("any") specified - not valid for writes)
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x01\x00\x03\x06\x0E",8,
                                    "[1] (fin,fir) WRITE {g10v1 qc=00 #3..6: 0 1 1 1}");
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x01\x00\x00\x08\x5E\x01",9,
                                    "[1] (fin,fir) WRITE {g10v1 qc=00 #0..8: 0 1 1 1 1 0 1 0 1}");
    check_parse_fail(dnp3_p_app_request, "\x01\x02\x0A\x01\x17\x00",6);
        // XXX is qc=17 (index prefixes) valid for bit-packed variation? which encoding is correct?
        //     e.g: "[1] (fin,fir) WRITE {g10v1 qc=17 #1:0,#4:1,#8:1}" "\x01\x02\x0A\x01\x17\x03...
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
    check_parse_fail(dnp3_p_app_request, "\xC1\x02\x0A\x01\x00\x00\x08\x5E\x02",9);
        // (an unused bit after the packed objects is not zero)
}

static void test_rsp_null(void)
{
    //check_parse(dnp3_p_app_response, "\xC2\x81\x00",2, "[2] RESPONSE");
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
    //g_test_add_func("/app/rsp/fail", test_rsp_fail);
    //g_test_add_func("/app/rsp/ac", test_rsp_ac);
    //g_test_add_func("/app/rsp/null", test_rsp_null);

    g_test_run();
}
