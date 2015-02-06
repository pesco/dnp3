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
      if (result->ast && ISERR(result->ast->token_type)) { \
        g_test_message("Check failed on line %d: shouldn't have succeeded, but parsed error %d", \
                       __LINE__, result->ast->token_type); \
      } else { \
        g_test_message("Check failed on line %d: shouldn't have succeeded, but did", \
                       __LINE__); \
      } \
      g_test_fail(); \
    } \
  } while(0)

#define check_parse_error(parser, input, inp_len, err) do { \
    HParseResult *res = h_parse(parser, (const uint8_t*)input, inp_len); \
    if (!res) { \
      g_test_message("Parse failed on line %d", __LINE__); \
      g_test_fail(); \
    } else { \
      if (!res->ast || !ISERR(res->ast->token_type)) { \
        g_test_message("Check failed on line %d: expected parse error (%d)", \
                       __LINE__, (int)err); \
        g_test_fail(); \
      } else { \
        check_cmp_uint64(res->ast->token_type, ==, err); \
      } \
      HArenaStats stats; \
      h_allocator_stats(res->arena, &stats); \
      g_test_message("Parse used %zd bytes, wasted %zd bytes. " \
                     "Inefficiency: %5f%%", \
		     stats.used, stats.wasted, \
		     stats.wasted * 100. / (stats.used+stats.wasted)); \
      h_delete_arena(res->arena); \
    } \
  } while(0)

#define check_parse(parser, input, inp_len, result) do { \
    HParseResult *res = h_parse(parser, (const uint8_t*)input, inp_len); \
    if (!res) { \
      g_test_message("Parse failed on line %d", __LINE__); \
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
    check_parse_error(dnp3_p_app_request, "\x00\x23",2, ERR_FUNC_NOT_SUPP);
    check_parse_error(dnp3_p_app_request, "\x00\x81",2, ERR_FUNC_NOT_SUPP);
}

static void test_req_ac(void)
{
    check_parse(dnp3_p_app_request, "\x00\x00",2, "[0] CONFIRM");
    check_parse(dnp3_p_app_request, "\x05\x00",2, "[5] CONFIRM");
    check_parse_fail(dnp3_p_app_request, "\x10\x00",2); // (uns)
    check_parse_fail(dnp3_p_app_request, "\x20\x00",2); // (con)
    check_parse_fail(dnp3_p_app_request, "\x30\x00",2); // (con,uns)
    check_parse(dnp3_p_app_request, "\x42\x00",2, "[2] (fir) CONFIRM");
    check_parse(dnp3_p_app_request, "\x83\x00",2, "[3] (fin) CONFIRM");
    check_parse(dnp3_p_app_request, "\xC0\x00",2, "[0] (fin,fir) CONFIRM");
}

static void test_req_ohdr(void)
{
    // truncated (otherwise valid) object header
    check_parse_fail(dnp3_p_app_request, "\x00\x01\x01",3);
    check_parse_fail(dnp3_p_app_request, "\x00\x01\x01\x00",4);
    check_parse_fail(dnp3_p_app_request, "\x00\x01\x01\x00\x17",5);
    check_parse_fail(dnp3_p_app_request, "\x00\x01\x01\x00\x17\x01",6);
    check_parse_fail(dnp3_p_app_request, "\x00\x01\x01\x00\x7A",5);

    // truncated object header (invalid group)
    check_parse_fail(dnp3_p_app_request, "\x00\x01\x05",3);
    check_parse_fail(dnp3_p_app_request, "\x00\x01\x05\x00",4);
    check_parse_fail(dnp3_p_app_request, "\x00\x01\x01\x00\x00\x03\x41\x58",8);
    check_parse_fail(dnp3_p_app_request, "\x00\x01\x01\x00\x06\x05\x00",7);

    // truncated object header (invalid variation)
    check_parse_fail(dnp3_p_app_request, "\x00\x01\x32\x00",4);

    // invalid group / variation (complete header)
    check_parse_error(dnp3_p_app_request, "\x00\x01\x05\x00\x00\x03\x41",7, ERR_OBJ_UNKNOWN);
    check_parse_error(dnp3_p_app_request, "\x00\x01\x32\x00\x06",5, ERR_OBJ_UNKNOWN);
}

static void test_req_confirm(void)
{
    check_parse(dnp3_p_app_request, "\x00\x00",2, "[0] CONFIRM");
    check_parse_fail(dnp3_p_app_request, "\x00\x00\x01\x00\x06",5);
}

static void test_req_read(void)
{
    check_parse(dnp3_p_app_request, "\x00\x01",2, "[0] READ");
    check_parse(dnp3_p_app_request, "\x00\x01\x01\x00\x17\x03\x41\x43\x42",9,
                                    "[0] READ {g1v0 qc=17 #65 #67 #66}");
    check_parse(dnp3_p_app_request, "\x00\x01\x01\x00\x17\x00",6,   // XXX is 0 a valid count?
                                    "[0] READ {g1v0 qc=17}");
    check_parse(dnp3_p_app_request, "\x00\x01\x01\x00\x00\x03\x41",7,
                                    "[0] READ {g1v0 qc=00 #3..65}");
    check_parse(dnp3_p_app_request, "\x00\x01\x02\x03\x00\x03\x41",7,
                                    "[0] READ {g2v3 qc=00 #3..65}");
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

    g_test_run();
}
