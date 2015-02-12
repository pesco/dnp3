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
    check_parse(dnp3_p_app_request, "\xC0\x23",2, "FUNC_NOT_SUPP on [0] (fin,fir) 0x23");
    check_parse(dnp3_p_app_request, "\xC0\x81",2, "FUNC_NOT_SUPP on [0] (fin,fir) RESPONSE");
}

static void test_req_ac(void)
{
    check_parse(dnp3_p_app_request, "\xC2\x00",2, "[2] (fin,fir) CONFIRM");
    check_parse(dnp3_p_app_request, "\xC5\x00",2, "[5] (fin,fir) CONFIRM");
    check_parse(dnp3_p_app_request, "\xD3\x00",2, "[3] (fin,fir,uns) CONFIRM");
    // XXX is it correct to fail the parse on invalid AC flags?
    check_parse_fail(dnp3_p_app_request, "\xE0\x23",2); // (con) ???
    check_parse_fail(dnp3_p_app_request, "\xE0\x81",2); // (con) RESPONSE
    check_parse(dnp3_p_app_request, "\xD0\x23",2, "FUNC_NOT_SUPP on [0] (fin,fir,uns) 0x23");
    check_parse(dnp3_p_app_request, "\xD0\x81",2, "FUNC_NOT_SUPP on [0] (fin,fir,uns) RESPONSE");
    check_parse_fail(dnp3_p_app_request, "\xE0\x23",2); // (con) ???
    check_parse_fail(dnp3_p_app_request, "\x80\x23",2); // not (fir) ???
    check_parse_fail(dnp3_p_app_request, "\xD0\x01",2); // (uns) READ
    check_parse_fail(dnp3_p_app_request, "\x40\x00",2); // not (fin)
    check_parse_fail(dnp3_p_app_request, "\x80\x00",2); // not (fir)
    check_parse_fail(dnp3_p_app_request, "\xE0\x00",2); // (con) CONFIRM
    check_parse_fail(dnp3_p_app_request, "\xF0\x00",2); // (con,uns)
    check_parse(dnp3_p_app_request, "\xC0\x00",2, "[0] (fin,fir) CONFIRM");
}

static void test_req_ohdr(void)
{
    // truncated (otherwise valid) object header
    check_parse(dnp3_p_app_request, "\xC0\x01\x01",3, "PARAM_ERROR on [0] (fin,fir) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00",4, "PARAM_ERROR on [0] (fin,fir) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17",5, "PARAM_ERROR on [0] (fin,fir) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17\x01",6, "PARAM_ERROR on [0] (fin,fir) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x7A",5, "PARAM_ERROR on [0] (fin,fir) READ");

    // truncated object header (invalid group)
    check_parse(dnp3_p_app_request, "\xC0\x01\x05",3, "PARAM_ERROR on [0] (fin,fir) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x05\x00",4, "PARAM_ERROR on [0] (fin,fir) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x00\x03\x41\x58",8, "PARAM_ERROR on [0] (fin,fir) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x06\x05\x00",7, "PARAM_ERROR on [0] (fin,fir) READ");

    // truncated object header (invalid variation)
    check_parse(dnp3_p_app_request, "\xC0\x01\x32\x00",4, "PARAM_ERROR on [0] (fin,fir) READ");

    // invalid group / variation (complete header)
    check_parse(dnp3_p_app_request, "\xC0\x01\x05\x00\x00\x03\x41",7, "OBJ_UNKNOWN on [0] (fin,fir) READ");
    check_parse(dnp3_p_app_request, "\xC0\x01\x32\x00\x06",5, "OBJ_UNKNOWN on [0] (fin,fir) READ");
}

static void test_req_confirm(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x00",2, "[0] (fin,fir) CONFIRM");
    check_parse(dnp3_p_app_request, "\xD0\x00",2, "[0] (fin,fir,uns) CONFIRM");
    check_parse(dnp3_p_app_request, "\xC0\x00\x01\x00\x06",5, "PARAM_ERROR on [0] (fin,fir) CONFIRM");
        // XXX should a message with unexpected objects yield OBJ_UNKNOWN?
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
}

static void test_req_write(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x02",2, "[0] (fin,fir) WRITE"); // XXX null WRITE valid?
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x00\x00\x03\x06",7, "OBJ_UNKNOWN on [1] (fin,fir) WRITE");
        // (variation 0 ("any") specified - not valid for writes)
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x01\x00\x03\x06\x0E",8,
                                    "[1] (fin,fir) WRITE {g10v1 qc=00 #3..6: 0 1 1 1}");
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
    check_parse(dnp3_p_app_response, "\xC2\x00\x00\x00",4, "FUNC_NOT_SUPP on [2] (fin,fir) CONFIRM");
    check_parse(dnp3_p_app_response, "\xC3\x01\x00\x00",4, "FUNC_NOT_SUPP on [3] (fin,fir) READ");
    check_parse(dnp3_p_app_response, "\xC0\x23\x00\x00",4, "FUNC_NOT_SUPP on [0] (fin,fir) 0x23");
    check_parse(dnp3_p_app_response, "\xC0\xF0\x00\x00",4, "FUNC_NOT_SUPP on [0] (fin,fir) 0xF0");
}

static void test_rsp_ac(void)
{
    check_parse(dnp3_p_app_response, "\x02\x81\x00\x00",4, "[2] RESPONSE");
    check_parse(dnp3_p_app_response, "\x22\x81\x00\x00",4, "[2] (con) RESPONSE");
    check_parse(dnp3_p_app_response, "\x43\x81\x00\x00",4, "[3] (fir) RESPONSE");
    check_parse(dnp3_p_app_response, "\x62\x81\x00\x00",4, "[2] (fir,con) RESPONSE");
    check_parse(dnp3_p_app_response, "\x82\x81\x00\x00",4, "[2] (fin) RESPONSE");
    check_parse(dnp3_p_app_response, "\xA4\x81\x00\x00",4, "[4] (fin,con) RESPONSE");
    check_parse(dnp3_p_app_response, "\xC2\x81\x00\x00",4, "[2] (fin,fir) RESPONSE");
    check_parse(dnp3_p_app_response, "\xE2\x81\x00\x00",4, "[2] (fin,fir,con) RESPONSE");

    check_parse(dnp3_p_app_response, "\x32\x82\x00\x00",4, "[2] (con,uns) UNSOLICITED_RESPONSE");
    check_parse(dnp3_p_app_response, "\x72\x82\x00\x00",4, "[2] (fir,con,uns) UNSOLICITED_RESPONSE");
    check_parse(dnp3_p_app_response, "\xB2\x82\x00\x00",4, "[2] (fin,con,uns) UNSOLICITED_RESPONSE");
    check_parse(dnp3_p_app_response, "\xF2\x82\x00\x00",4, "[2] (fin,fir,con,uns) UNSOLICITED_RESPONSE");

    // XXX should unsol. responses with con=0 really be discarded?
    check_parse_fail(dnp3_p_app_response, "\x13\x82\x00\x00",4); // (uns)
    check_parse_fail(dnp3_p_app_response, "\x54\x82\x00\x00",4); // (fir,uns)
    check_parse_fail(dnp3_p_app_response, "\x9A\x82\x00\x00",4); // (fin,uns)
    check_parse_fail(dnp3_p_app_response, "\xD0\x82\x00\x00",4); // (fin,fir,uns)

    check_parse_fail(dnp3_p_app_response, "\x13\x81\x00\x00",4); // (uns)
    check_parse_fail(dnp3_p_app_response, "\x32\x81\x00\x00",4); // (con,uns)
    check_parse_fail(dnp3_p_app_response, "\x54\x81\x00\x00",4); // (fir,uns)
    check_parse_fail(dnp3_p_app_response, "\x72\x81\x00\x00",4); // (fir,con,uns)
    check_parse_fail(dnp3_p_app_response, "\x9A\x81\x00\x00",4); // (fin,uns)
    check_parse_fail(dnp3_p_app_response, "\xB2\x81\x00\x00",4); // (fin,con,uns)
    check_parse_fail(dnp3_p_app_response, "\xD0\x81\x00\x00",4); // (fin,fir,uns)
    check_parse_fail(dnp3_p_app_response, "\xF2\x81\x00\x00",4); // (fin,fir,con,uns)
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
    check_parse(dnp3_p_app_response, "\xC2\x81\x00\x00",4, "[2] (fin,fir) RESPONSE");
}

static void test_obj_binin(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x00\x03\x08",7,
                                    "[0] (fin,fir) READ {g1v0 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x01\x00\x17\x02\x03\x08",8,
                                    "[0] (fin,fir) READ {g1v0 qc=17 #3 #8}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x01\x00\x03\x08\x19",10,
                                     "[0] (fin,fir) RESPONSE {g1v1 qc=00 #3..8: 1 0 0 1 1 0}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x01\x17\x00",8,          // invalid qc
                                     "PARAM_ERROR on [0] (fin,fir) RESPONSE");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x02\x17\x01\x03\x80",10,
                                     "[0] (fin,fir) RESPONSE {g1v2 qc=17 #3:1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x02\x17\x01\x03\x83",10,
                                     "[0] (fin,fir) RESPONSE {g1v2 qc=17 #3:(online,restart)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x02\x17\x01\x03\x89",10,
                                     "[0] (fin,fir) RESPONSE {g1v2 qc=17 #3:(online,remote_forced)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x01\x02\x17\x01\x03\x40",10,  // reserved bit set
                                     "PARAM_ERROR on [0] (fin,fir) RESPONSE");
}

static void test_obj_bininev(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01\x02\x00\x00\x03\x08",7,
                                    "[0] (fin,fir) READ {g2v0 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x02\x01\x00\x03\x08",7,
                                    "[0] (fin,fir) READ {g2v1 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x02\x03\x00\x03\x08",7,
                                    "[0] (fin,fir) READ {g2v3 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x02\x04\x00\x03\x08",7,
                                    "OBJ_UNKNOWN on [0] (fin,fir) READ");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x01\x17\x01\x03\x80",10,
                                     "[0] (fin,fir) RESPONSE {g2v1 qc=17 #3:1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x01\x17\x01\x03\x83",10,
                                     "[0] (fin,fir) RESPONSE {g2v1 qc=17 #3:(online,restart)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x01\x17\x01\x03\xC3",10,
                                     "PARAM_ERROR on [0] (fin,fir) RESPONSE");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x02\x17\x01\x03\x80\x00\x00\x00\x00\x00\x00",16,
                                     "[0] (fin,fir) RESPONSE {g2v2 qc=17 #3:1@0}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x02\x17\x01\x03\x01\x00\x00\x00\x00\x00\x80",16,
                                     "[0] (fin,fir) RESPONSE {g2v2 qc=17 #3:(online)0@140737488355.328}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x02\x17\x01\x03\x82\xA0\xFC\x7D\x7A\x4B\x01",16,
                                     "[0] (fin,fir) RESPONSE {g2v2 qc=17 #3:(restart)1@1423689252}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x02\x17\x01\x03\xC2\x00\x00\x00\x00\x00\x00",16,
                                     "PARAM_ERROR on [0] (fin,fir) RESPONSE");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x03\x17\x01\x03\x80\x00\x00",12,
                                     "[0] (fin,fir) RESPONSE {g2v3 qc=17 #3:1@+0}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x03\x17\x01\x03\x01\x00\x80",12,
                                     "[0] (fin,fir) RESPONSE {g2v3 qc=17 #3:(online)0@+32.768}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x03\x17\x01\x03\x81\xE0\x56",12,
                                     "[0] (fin,fir) RESPONSE {g2v3 qc=17 #3:(online)1@+22.240}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x02\x03\x17\x01\x03\xC1\x00\x00",12,
                                     "PARAM_ERROR on [0] (fin,fir) RESPONSE");
        // XXX should the relative time variants generate a PARAM_ERROR unless they are preceded by a
        //     Common Time-of-Occurance (CTO, group 50) object in the same message?
}

static void test_obj_dblbitin(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01\x03\x00\x00\x03\x08",7,
                                    "[0] (fin,fir) READ {g3v0 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x03\x01\x00\x03\x08",7,
                                    "[0] (fin,fir) READ {g3v1 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x03\x02\x00\x03\x08",7,
                                    "[0] (fin,fir) READ {g3v2 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x03\x03\x00\x03\x08",7,
                                    "OBJ_UNKNOWN on [0] (fin,fir) READ");

    // v1 (packed)
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x01\x00\x00\x03\x36",10,
                                     "[0] (fin,fir) RESPONSE {g3v1 qc=00 #0..3: 1 0 - ~}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x01\x00\x00\x02\x06",10,
                                     "[0] (fin,fir) RESPONSE {g3v1 qc=00 #0..2: 1 0 ~}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x01\x00\x00\x02\x46",10,
                                     "PARAM_ERROR on [0] (fin,fir) RESPONSE");  // extra bit set

    // v2 (flags)
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x02\x17\x01\x03\x80",10,
                                     "[0] (fin,fir) RESPONSE {g3v2 qc=17 #3:1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x02\x17\x01\x03\x03",10,
                                     "[0] (fin,fir) RESPONSE {g3v2 qc=17 #3:(online,restart)~}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x02\x17\x01\x03\xA1",10,
                                     "[0] (fin,fir) RESPONSE {g3v2 qc=17 #3:(online,chatter_filter)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x03\x02\x17\x01\x03\xC1",10,
                                     "[0] (fin,fir) RESPONSE {g3v2 qc=17 #3:(online)-}");
}

static void test_obj_dblbitinev(void)
{
    check_parse(dnp3_p_app_request, "\xC0\x01\x04\x00\x00\x03\x08",7,
                                    "[0] (fin,fir) READ {g4v0 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x04\x01\x00\x03\x08",7,
                                    "[0] (fin,fir) READ {g4v1 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x04\x03\x00\x03\x08",7,
                                    "[0] (fin,fir) READ {g4v3 qc=00 #3..8}");
    check_parse(dnp3_p_app_request, "\xC0\x01\x04\x04\x00\x03\x08",7,
                                    "OBJ_UNKNOWN on [0] (fin,fir) READ");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x01\x17\x01\x03\x80",10,
                                     "[0] (fin,fir) RESPONSE {g4v1 qc=17 #3:1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x01\x17\x01\x03\x03",10,
                                     "[0] (fin,fir) RESPONSE {g4v1 qc=17 #3:(online,restart)~}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x01\x17\x01\x03\xC1",10,
                                     "[0] (fin,fir) RESPONSE {g4v1 qc=17 #3:(online)-}");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x02\x17\x01\x03\x80\x00\x00\x00\x00\x00\x00",16,
                                     "[0] (fin,fir) RESPONSE {g4v2 qc=17 #3:1@0}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x02\x17\x01\x03\x41\x00\x00\x00\x00\x00\x80",16,
                                     "[0] (fin,fir) RESPONSE {g4v2 qc=17 #3:(online)0@140737488355.328}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x02\x17\x01\x03\x02\xA0\xFC\x7D\x7A\x4B\x01",16,
                                     "[0] (fin,fir) RESPONSE {g4v2 qc=17 #3:(restart)~@1423689252}");

    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x03\x17\x01\x03\x80\x00\x00",12,
                                     "[0] (fin,fir) RESPONSE {g4v3 qc=17 #3:1@+0}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x03\x17\x01\x03\x41\x00\x80",12,
                                     "[0] (fin,fir) RESPONSE {g4v3 qc=17 #3:(online)0@+32.768}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x04\x03\x17\x01\x03\x81\xE0\x56",12,
                                     "[0] (fin,fir) RESPONSE {g4v3 qc=17 #3:(online)1@+22.240}");
        // XXX should the relative time variants generate a PARAM_ERROR unless they are preceded by a
        //     Common Time-of-Occurance (CTO, group 50) object in the same message?
}

static void test_obj_binout(void)
{
    // v1 (packed)
    check_parse(dnp3_p_app_request, "\xC0\x01\x0A\x01\x00\x03\x41",7,
                                    "[0] (fin,fir) READ {g10v1 qc=00 #3..65}");
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x01\x00\x00\x08\x5E\x01",9,
                                    "[1] (fin,fir) WRITE {g10v1 qc=00 #0..8: 0 1 1 1 1 0 1 0 1}");
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x01\x17\x00",6, "PARAM_ERROR on [1] (fin,fir) WRITE");
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
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x01\x00\x00\x08\x5E\x02",9, "PARAM_ERROR on [1] (fin,fir) WRITE");
        // (an unused bit after the packed objects is not zero)
    check_parse(dnp3_p_app_response, "\xC1\x81\x00\x00\x0A\x01\x00\x00\x08\x5E\x01",11,
                                     "[1] (fin,fir) RESPONSE {g10v1 qc=00 #0..8: 0 1 1 1 1 0 1 0 1}");
    check_parse(dnp3_p_app_response, "\xC1\x81\x00\x00\x0A\x01\x00\x00\x08\x5E\x02",11,
                                     "PARAM_ERROR on [1] (fin,fir) RESPONSE");
        // (an unused bit after the packed objects is not zero)

    // v2 (flags)
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0A\x02\x17\x01\x03\x80",10,
                                     "[0] (fin,fir) RESPONSE {g10v2 qc=17 #3:1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0A\x02\x17\x01\x03\x83",10,
                                     "[0] (fin,fir) RESPONSE {g10v2 qc=17 #3:(online,restart)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0A\x02\x17\x01\x03\x89",10,
                                     "[0] (fin,fir) RESPONSE {g10v2 qc=17 #3:(online,remote_forced)1}");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0A\x02\x17\x01\x03\x20",10,  // reserved bit set
                                     "PARAM_ERROR on [0] (fin,fir) RESPONSE");
    check_parse(dnp3_p_app_response, "\xC0\x81\x00\x00\x0A\x02\x17\x01\x03\x40",10,  // reserved bit set
                                     "PARAM_ERROR on [0] (fin,fir) RESPONSE");
    check_parse(dnp3_p_app_request, "\xC1\x02\x0A\x02\x17\x01\x03\x80",9,
                                    "OBJ_UNKNOWN on [1] (fin,fir) WRITE");
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
    g_test_add_func("/app/rsp/fail", test_rsp_fail);
    g_test_add_func("/app/rsp/ac", test_rsp_ac);
    g_test_add_func("/app/rsp/iin", test_rsp_iin);
    g_test_add_func("/app/rsp/null", test_rsp_null);
    g_test_add_func("/app/obj/binin", test_obj_binin);
    g_test_add_func("/app/obj/bininev", test_obj_bininev);
    g_test_add_func("/app/obj/dblbitin", test_obj_dblbitin);
    g_test_add_func("/app/obj/dblbitinev", test_obj_dblbitinev);
    g_test_add_func("/app/obj/binout", test_obj_binout);

    g_test_run();
}
