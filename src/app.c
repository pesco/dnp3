// application layer

#include <dnp3.h>
#include <hammer/glue.h>
#include "hammer.h" // XXX placeholder for extensions
#include <string.h>
#include <stdlib.h> // malloc
#include <inttypes.h> // PRIu32 etc.
#include "util.h"
#include "g1_binin.h"
#include "g2_bininev.h"
#include "g3_dblbitin.h"
#include "g4_dblbitinev.h"
#include "g120_auth.h"


/// APPLICATION HEADER ///


/// AGGRESSIVE-MODE AUTHENTICATION ///

static HParsedToken *act_with_ama(const HParseResult *p, void *env)
{
    DNP3_Request *r = H_ALLOC(DNP3_Request);
    DNP3_AuthData *a = H_ALLOC(DNP3_AuthData);

    // XXX this is wrong; the incoming objects are raw, not yet a DNP3_Request
    //     leave all this to act_req_odata!?

    // input is a sequence: (auth_aggr, request, auth_mac)
    *r = *H_FIELD(DNP3_Request, 1);

    // XXX fill AuthData from H_FIELD(..., 0) and H_FIELD(..., 2)
    // a->xxx = H_FIELD(..., 1)->xxx;
    // a->mac = H_FIELD(..., 2);
    r->auth = a;

    return H_MAKE(DNP3_Request, r);
}

// combinator: allow aggresive-mode auth objects around base parser
static HParser *ama(HParser *base)
{
    // aggressive mode objects are optional, but if used:
    // g120v3 (aggressive mode request) must be the first object.
    // g120v9 (message authentication code) must be the last object.

    H_ARULE(with_ama, h_sequence(dnp3_p_g120v3_auth_aggr_block,
                                 base,
                                 dnp3_p_g120v9_auth_mac_block, NULL));
        // XXX parse/validate mac before rest of odata?!

    return h_choice(with_ama, base, NULL);
}


/// OBJECT DATA ///

// object data parsers, indexed by function code
static HParser *odata[256] = {NULL};

// XXX note about errors:
// dnp3 has three types of errors (cf. dnp3.h):
// FUNC_NOT_SUPP, OBJ_UNKNOWN, PARAM_ERROR.
//
// PARAM_ERROR is the catch-all.
// FUNC_NOT_SUPP is raised when the function code is unexpected.
// OBJ_UNKNOWN is raised when a group/variation is unexpected.
//
// so we need to distinguish these cases in the parser. here is how:
// the parsers for each individual type (group/variation) of object fail when
// they do not see the expected group/variation. when anything ELSE (after
// those first two bytes) fails, they are expected to yield a
// TT_PARAM_ERROR result (cf. h_error). this means that an h_choice of
// different types of objects will abort with that error.
// in case none of the branches match, the h_choice has the OBJ_UNKNOWN case as
// a catch-all. this must be the case for all such h_choices. we use the
// dnp3_p_objchoice combinator to abstract that.

HParser *dnp3_p_app_request;

static HParser *range_index;
static HParser *range_addr;
static HParser *range_none;
static HParser *range_count;
static HParser *range_count1;
static HParser *range_vfcount;
static HParser *range_vfcount1;

static HParser *ohdr_irange;
static HParser *ohdr_arange;
static HParser *ohdr_all;
static HParser *ohdr_count;
static HParser *ohdr_count1;

static HParser *rblock_;

// prefix code
static HParser *withpc(uint8_t x, HParser *p)
{
    HParser *pc = h_int_range(h_right(dnp3_p_reserved(1), h_bits(3, 0)), x, x);

    return h_sequence(pc, p, NULL);
}
static HParser *noprefix(HParser *p)
{
    return withpc(0, p);
}

// range specifier code
static HParser *rsc(uint8_t x)
{
    HParser *p = h_int_range(h_bits(4, 0), x, x);

    // funnel the rsc out via h_put_value so we can find and save it in the
    // DNP3_ObjectBlock struct later
    return h_put_value(p, "rsc");
}

// range fields giving an actual range
static bool validate_range(HParseResult *p, void *user)
{
    // p->ast = (start, stop)
    uint32_t start = H_FIELD_UINT(0);
    uint32_t stop  = H_FIELD_UINT(1);

    // validate that start <= stop
    // validate that count (stop - start + 1) will fit in size_t
    return (start <= stop && stop-start < SIZE_MAX);
}
static HParsedToken *act_range(const HParseResult *p, void *user)
{
    // p->ast = (start, stop)
    uint32_t start = H_FIELD_UINT(0);
    uint32_t stop  = H_FIELD_UINT(1);

    assert(start <= stop);
    assert(stop - start < SIZE_MAX);
    return H_MAKE_UINT(stop - start + 1);

}
static HParser *range(uint8_t x, HParser *p)
{
    H_RULE  (start, h_put_value(p, "range_base"));
    H_RULE  (stop,  p);
    H_AVRULE(range, h_sequence(start, stop, NULL));

    return h_right(rsc(x), range);
}

// range fields giving a count
static HParser *count(uint8_t x, HParser *p)
{
    return h_right(rsc(x), p);
}

// helper for actions below
HParsedToken *count_idxs_objs(HArena *arena, size_t count, uint32_t *idxs, DNP3_Object *objs)
{
    HParsedToken *res = h_make_seqn(arena, 3);
    h_seq_snoc(res, h_make_uint(arena, count));
    h_seq_snoc(res, h_make(arena, TT_USER, idxs));
    h_seq_snoc(res, h_make(arena, TT_DNP3_Object, objs));
    return res;
}

// semantic actions to generate the (count,idxs,objs) triple in different cases
static HParsedToken *act_indexes_objects(const HParseResult *p, void *user)
{
    // p->ast = ((idx,obj)...)
    size_t n = h_seq_len(p->ast);

    uint32_t    *indexes = NULL;
    DNP3_Object *objects = NULL;
    if(n > 0) {
        indexes = h_arena_malloc(p->arena, 4*n);
        objects = h_arena_malloc(p->arena, sizeof(DNP3_Object) * n);

        for(size_t i=0; i<n; i++) {
            HParsedToken *i_o = h_seq_index(p->ast, i);

            indexes[i] = H_INDEX_UINT(i_o, 0);
            objects[i] = *H_INDEX(DNP3_Object, i_o, 1);
        }
    }

    return count_idxs_objs(p->arena, n, indexes, objects);
}
static HParsedToken *act_indexes_only(const HParseResult *p, void *user)
{
    // p->ast = (idx...)
    size_t n = h_seq_len(p->ast);

    uint32_t *indexes = NULL;
    if(n > 0) {
        indexes = h_arena_malloc(p->arena, 4*n);

        for(size_t i=0; i<n; i++) {
            indexes[i] = H_FIELD_UINT(i);
        }
    }

    return count_idxs_objs(p->arena, n, indexes, NULL);
}
static HParsedToken *act_objects_only(const HParseResult *p, void *user)
{
    // p->ast = (obj...)
    size_t n = h_seq_len(p->ast);

    DNP3_Object *objects = NULL;
    if(n > 0) {
        objects = h_arena_malloc(p->arena, sizeof(DNP3_Object) * n);

        for(size_t i=0; i<n; i++) {
            objects[i] = *H_FIELD(DNP3_Object, i);
        }
    }

    return count_idxs_objs(p->arena, n, NULL, objects);
}

static HParser *prefixed_index(HParser *idx, HParser *p)
{
    HParser *obj;
    HAction act;

    if(p) {
        obj = h_sequence(idx, p, NULL);
        act = act_indexes_objects;
    } else {
        obj = idx;
        act = act_indexes_only;
    }

    return h_action(h_length_value(range_count, obj), act, NULL);
}

static HParser *f_bindvf(const HParsedToken *n, void *user)
{
    HParser *(*q)(size_t) = user;

    return q(H_CAST_UINT(n));
}
static HParser *prefixed_size(HParser *vfcnt, HParser *p, HParser *(*q)(size_t))
{
    return h_action(h_length_value(vfcnt, h_bind(p, f_bindvf, q)),
                    act_objects_only, NULL);
}


static HParser *oblock_index(HParser *p)
{
    return h_choice(withpc(1, prefixed_index(h_uint8(), p)), 
                    withpc(2, prefixed_index(h_uint16(), p)),
                    withpc(3, prefixed_index(h_uint32(), p)), NULL);
}

static HParser *oblock_vf_(HParser *cnt, HParser *(*p)(size_t))
{
    return h_choice(withpc(4, prefixed_size(cnt, h_uint8(), p)), 
                    withpc(5, prefixed_size(cnt, h_uint16(), p)),
                    withpc(6, prefixed_size(cnt, h_uint32(), p)), NULL);
}

static HParser *oblock_vf(HParser *(*p)(size_t))
{
    return oblock_vf_(range_vfcount, p);
}

static void init_oblock(void)
{
    // qualifier codes and their meanings:
    //
    //   0[0-2]      index range
    //   0[3-5]      address range
    //   06          "all" ((read?) requests only)
    //   0[7-9]      (max) count
    //   [1-3][7-9]  index list (count + index prefix)
    //   [4-6]B      "variable format" (count + size prefix)

    range_index =  h_choice(range(0x0, h_uint8()),
                            range(0x1, h_uint16()),
                            range(0x2, h_uint32()), NULL);
    range_addr =   h_choice(range(0x3, h_uint8()),
                            range(0x4, h_uint16()),
                            range(0x5, h_uint32()), NULL);
    range_none =              rsc(0x6);             // no range field
    range_count =  h_choice(count(0x7, h_uint8()),
                            count(0x8, h_uint16()),
                            count(0x9, h_uint32()), NULL);
    range_count1 =          count(0x7, h_ch(1)),    // a single object
                               // 0xA = reserved
    range_vfcount =         count(0xB, h_uint8());  // count of var-size objects
    range_vfcount1 =        count(0xB, h_ch(1));    // a single var-size object

    ohdr_irange = noprefix(range_index);
    ohdr_arange = noprefix(range_addr);
    ohdr_all    = noprefix(range_none);
    ohdr_count  = noprefix(range_count);

    H_RULE(rblock_index, oblock_index(NULL));
    rblock_ = h_choice(ohdr_irange, ohdr_arange, ohdr_all, ohdr_count,
                       rblock_index, NULL);
}

HParser *group(DNP3_Group g)
{
    return h_ch(g);
}

HParser *variation(DNP3_Variation v)
{
    return h_ch(v);
}

static HParsedToken *act_block(const HParseResult *p, void *user)
{
    DNP3_ObjectBlock *ob = H_ALLOC(DNP3_ObjectBlock);
    HParsedToken *tok;

    // expected structure:
    // p = (grp,var,((pc,(count,idxs,objs)),rsc,base))
    //   | (grp,var,((pc,count),rsc,base))              -- plain ohdr case
    //   | (grp,var,error)

    // propagate TT_ERR on the third place
    if(ISERR(H_INDEX_TOKEN(p->ast, 2)->token_type))
        return H_INDEX_TOKEN(p->ast, 2);

    ob->group = H_FIELD_UINT(0);
    ob->variation = H_FIELD_UINT(1);
    ob->prefixcode = H_FIELD_UINT(2, 0, 0);
    ob->rangespec = H_FIELD_UINT(2, 1);

    // range base (can be TT_NONE)
    tok = H_INDEX_TOKEN(p->ast, 2, 2);
    if(tok->token_type == TT_NONE) {
        ob->range_base = 0;
    } else {
        ob->range_base = H_CAST_UINT(tok);
    }

    // objects and indexes
    tok = H_INDEX_TOKEN(p->ast, 2, 0, 1);
    if(tok->token_type == TT_SEQUENCE) {
        // tok = (count,idxs,objs)
        ob->count   = H_INDEX_UINT(tok, 0);
        ob->indexes = h_assert_type(TT_USER, H_INDEX_TOKEN(tok, 1))->user;
        ob->objects = H_INDEX(DNP3_Object, tok, 2);
    } else {
        // tok = count
        ob->count = H_CAST_UINT(tok);
    }

    return H_MAKE(DNP3_ObjectBlock, ob);
}

// generic parser for object blocks
static HParser *block(HParser *grp, HParser *var, HParser *block_)
{
    H_RULE (rsc,    h_get_value("rsc"));
    H_RULE (base,   h_optional(h_get_value("range_base")));

    // the error propagation dance
    // we want a parse failure in group and variation to lead to failure and
    // one in the rest to yield PARAM_ERROR.
    H_RULE (rest,   h_sequence(block_, rsc, base, NULL));
    H_RULE (e_rest, h_choice(rest, h_error(ERR_PARAM_ERROR), NULL));
    H_ARULE(block,  h_sequence(grp, var, e_rest, NULL));

    return block;
}

HParser *dnp3_p_rblock(DNP3_Group g, ...)
{
    va_list args;
    int n=0, i;
    HParser **vs;

    // count arguments
    va_start(args, g);
    while(va_arg(args, DNP3_Variation)) n++;
    va_end(args);

    // assemble array of parsers for the given variations 
    vs = malloc((n+2) * sizeof(HParser *));
    vs[0] = variation(DNP3_VARIATION_ANY);
    va_start(args, g);
    for(i=1; i<=n; i++)
        vs[i] = variation(va_arg(args, DNP3_Variation));
    va_end(args);
    vs[i] = NULL;

    return block(group(g), h_choice__a((void **)vs), rblock_);
}

HParser *dnp3_p_single(DNP3_Group g, DNP3_Variation v, HParser *obj)
{
    return block(group(g), variation(v),
                 noprefix(h_right(range_count1, obj)));
}

HParser *dnp3_p_single_vf(DNP3_Group g, DNP3_Variation v, HParser *(*obj)(size_t))
{
    return block(group(g), variation(v), oblock_vf_(range_vfcount1, obj));
}

static void init_odata(void)
{
    H_RULE(confirm, h_epsilon_p());

        // read_ohdr:
        //   may use variation 0 (any)
        //   may use group 60 (event class data)
        //   may use range specifier 0x6 (all objects)
    H_RULE (read_ohdr,  dnp3_p_objchoice(//dnp3_p_attr_rblock,   // device attributes

                                         dnp3_p_binin_rblock,    // binary inputs
                                         dnp3_p_bininev_rblock,
                                         dnp3_p_dblbitin_rblock,
                                         dnp3_p_dblbitinev_rblock,

                                 //g10...,    // binary outputs
                                 //g11...,
                                 //g13...,

                                 //g20...,    // counters
                                 //g21...,
                                 //g22...,
                                 //g23...,

//                                 g30...,    // analog inputs
//                                 g31...,
//                                 g32...,
//                                 g33...,
//                                 g34...,
//
//                                 g40...,    // analog outputs
//                                 g41...,
//                                 g42...,
//                                 g43...,
//
//                                 g50v1...,  // times   XXX single object (qc 07, count 1)
//                                 g50v4...,
//
//                                 g60...,    // event class data
//
//                                 g70v5...,  // files   XXX oblock!!!
//                                 g70v6...,
//
//                                 g80...,    // internal indications
//                                 g81...,    // device storage
//                                 
//                                 g83...,    // data sets
//                                 g85...,
//                                 g86v1...,
//                                 g86v2...,
//                                 g86v3...,
//                                 g87...,
//                                 g88...,
//
//                                 g101...,   // bcd
//                                 g102...,   // octet
//                                 g110...,   // octet string
//                                 g111...,   // octet string event
//                                 g113v0..., // virtual terminal events
//
//                                 g121...,   // security statistic
//                                 g122...,

                                 NULL));
    H_RULE (read,       dnp3_p_many(read_ohdr));
    // XXX NB parsing pseudocode in AN2012-004b does NOT work for READ requests.
    //     it misses the case that a function code requires object headers
    //     but no objects. never mind that it might require an object with some
    //     variations but not others (e.g. g70v5 file transmission).

    odata[DNP3_CONFIRM] = ama(confirm);
    odata[DNP3_READ]    = ama(read);
    //odata[DNP3_WRITE]   = ama(write);

        // read_rsp_object:
        //   may not use variation 0
        //   may not use group 60
        //   may not use range specifier 0x6
    //odata [DNP3_RESPONSE] = ama(response);    // XXX ? or depend on req. fc?!

    //odata[DNP3_AUTHENTICATE_REQ]    = authenticate_req;
    //odata[DNP3_AUTH_REQ_NO_ACK]     = auth_req_no_ack;
}


/// APPLICATION LAYER FRAGMENTS ///

// combine header, auth, and object data into final DNP3_Request
static HParsedToken *act_req_odata(const HParseResult *p, void *user)
{
    const HParsedToken *hdr = user; // hdr = (ac, fc)

    // propagate TT_ERR on objects
    if(p->ast && ISERR(p->ast->token_type)) {
        return p->ast;  // XXX copy?
    }

    DNP3_Request *req = H_ALLOC(DNP3_Request);
    req->ac = *H_INDEX(DNP3_AppControl, hdr, 0);
    req->fc = H_INDEX_UINT(hdr, 1);
    req->auth = NULL;

    // copy object data. form of AST expected:
    //  (authdata, (oblock...))
    //  (authdata, oblock)
    //  (authdata, [null])
    //  (oblock...)
    //  oblock
    //  [null]

    const HParsedToken *od = p->ast;

    // remove leading authdata if present
    if(od && od->token_type == TT_SEQUENCE
          && od->seq->used > 0
          && od->seq->elements[0]->token_type == TT_DNP3_AuthData)
    {
        req->auth = H_INDEX(DNP3_AuthData, od, 0);
        od = H_INDEX_TOKEN(od, 1);
    }

    if(od == NULL) {
        // empty case (no odata)
        req->odata = NULL;
    } else if(od->token_type == TT_SEQUENCE) {
        // extract object blocks
        size_t n = od->seq->used;
        req->nblocks = n;
        req->odata = h_arena_malloc(p->arena, sizeof(DNP3_ObjectBlock *) * n);
        for(size_t i=0; i<req->nblocks; i++) {
            req->odata[i] = H_INDEX(DNP3_ObjectBlock, od, i);
        }
    } else {
        // single-oblock case
        req->nblocks = 1;
        req->odata = H_ALLOC(DNP3_ObjectBlock *);
        req->odata[0] = H_CAST(DNP3_ObjectBlock, od);
    }

    return H_MAKE(DNP3_Request, req);
}

// parse the rest of a request, after the application header
static HParser *f_app_request(const HParsedToken *hdr, void *env)
{
    // propagate TT_ERR on function code
    HParsedToken *fc_ = H_INDEX_TOKEN(hdr, 1);
    if(ISERR(H_INDEX_TOKEN(hdr, 1)->token_type)) {
        return h_unit(fc_);
    }

    int fc = H_CAST_UINT(fc_);

    // basic object data parser
    HParser *p = odata[fc];
    assert(p != NULL);

    return h_action(p, act_req_odata, (void *)hdr);
}

static HParsedToken *act_ac(const HParseResult *p, void *user)
{
    DNP3_AppControl *ac = H_ALLOC(DNP3_AppControl);

    ac->fin = H_FIELD_UINT(0, 0);
    ac->fir = H_FIELD_UINT(0, 1);
    ac->con = H_FIELD_UINT(0, 2);
    ac->uns = H_FIELD_UINT(0, 3);
    ac->seq = H_FIELD_UINT(1);

    return H_MAKE(DNP3_AppControl, ac);
}

#define act_reqac act_ac
#define act_rspac act_ac

void dnp3_p_init_app(void)
{
    // initialize object block and associated parsers/combinators
    init_oblock();

    // initialize object parsers
    dnp3_p_init_g1_binin();
    dnp3_p_init_g2_bininev();
    dnp3_p_init_g3_dblbitin();
    dnp3_p_init_g4_dblbitinev();

    // initialize request-specific "object data" parsers
    init_odata();

    H_RULE (bit,    h_bits(1, false));
    H_RULE (zro,    h_int_range(bit, 0, 0));
    H_RULE (fin,    bit);
    H_RULE (fir,    bit);
    H_RULE (con,    bit);
    H_RULE (uns,    bit);
    H_RULE (reqflags, h_sequence(fin,fir,zro,zro, NULL));
    H_RULE (rspflags, h_sequence(fin,fir,con,uns, NULL));

    H_RULE (seqno,  h_bits(4, false));
    H_ARULE(reqac,  h_sequence(reqflags, seqno, NULL));
    H_ARULE(rspac,  h_sequence(rspflags, seqno, NULL));
    H_RULE (iin,    h_sequence(h_bits(14, false), dnp3_p_reserved(2), NULL)); // XXX individual bits?

    H_RULE (fc,     h_uint8());
    H_RULE (errfc,  h_right(fc, h_error(ERR_FUNC_NOT_SUPP)));
    H_RULE (reqfc,  h_choice(h_int_range(fc, 0x00, 0x21), errfc, NULL));
    H_RULE (rspfc,  h_choice(h_int_range(fc, 0x81, 0x83), errfc, NULL));

    H_RULE (req_header, h_sequence(reqac, reqfc, NULL));
    H_RULE (rsp_header, h_sequence(rspac, rspfc, iin, NULL));

    H_RULE (request, h_bind(req_header, f_app_request, NULL));

    dnp3_p_app_request = dnp3_p_packet(request);
    // XXX response
}


/// human-readable output formatting ///

static char *funcname[] = {
    "CONFIRM", "READ", "WRITE", "SELECT", "OPERATE", "DIRECT_OPERATE",
    "DIRECT_OPERATE_NR", "IMMED_FREEZE", "IMMED_FREEZE_NR", "FREEZE_CLEAR",
    "FREEZE_CLEAR_NR", "FREEZE_AT_TIME", "FREEZE_AT_TIME_NR", "COLD_RESTART",
    "WARM_RESTART", "INITIALIZE_DATA", "INITIALIZE_APPL", "START_APPL",
    "STOP_APPL", "SAVE_CONFIG", "ENABLE_UNSOLICITED", "DISABLE_UNSOLICITED",
    "ASSIGN_CLASS", "DELAY_MEASURE", "RECORD_CURRENT_TIME", "OPEN_FILE",
    "CLOSE_FILE", "DELETE_FILE", "GET_FILE_INFO", "AUTHENTICATE_FILE",
    "ABORT_FILE", "ACTIVATE_CONFIG", "AUTHENTICATE_REQ", "AUTH_REQ_NO_ACK"
    };

int appendf(char **s, size_t *size, const char *fmt, ...)
{
    va_list args;
    size_t len = strlen(*s);
    char *p;

    va_start(args, fmt);
    while(1) {
        size_t left = *size - len;
        int n = vsnprintf(*s + len, left, fmt, args);
        if(n < 0) {
            va_end(args);
            return -1;
        }
        if(n < left)
            break;

        // need more space
        n = len + n + 1;
        p = realloc(*s, n);
        if(!p) {
            va_end(args);
            return -1;
        }
        *size = n;
        *s = p;
    }
    va_end(args);

    return 0;
}

char *dnp3_format_oblock(const DNP3_ObjectBlock *ob)
{
    size_t size = 128;
    char *res = malloc(size);
    const char *sep = ob->objects ? ":" : "";
    int x;

    if(!res) return NULL;
    res[0] = '\0';

    // group, variation, qc
    x = appendf(&res, &size, "g%dv%d qc=%x%x",
                (int)ob->group, (int)ob->variation,
                (unsigned int)ob->prefixcode, (unsigned int)ob->rangespec);
    if(x<0) goto err;

    // range
    if(ob->rangespec < 6) {
        uint32_t start = ob->range_base;
        uint32_t stop = start - 1 + ob->count;

        const char *fmt;
        if(ob->rangespec < 3) {
            fmt = " #%"PRIu32"..%"PRIu32"%s"; // index range
        } else {
            fmt = " @%"PRIx32"..%"PRIx32"%s"; // address range
        }

        x = appendf(&res, &size, fmt, start, stop, sep);
        if(x<0) goto err;
    }

    // objects/indexes
    if(ob->indexes || ob->objects) {
        for(size_t i=0; i<ob->count; i++) {
            if(appendf(&res, &size, " ") < 0) goto err;
            if(ob->indexes) {
                x = appendf(&res, &size, "#%"PRIu32"%s", ob->indexes[i], sep);
                if(x<0) goto err;
            }
            if(ob->objects) {
                if(appendf(&res, &size, ".") < 0) goto err; // XXX
            }
        }
    }

    return res;

err:
    free(res);
    return NULL;
}

char *dnp3_format_request(const DNP3_Request *req)
{
    char *odata = NULL;
    char *blk = NULL;
    char *res = NULL;
    size_t size;
    char *p;

    // flags string
    char flags[20]; // need 4*3(names)+3(seps)+2(parens)+1(space)+1(null)
    p = flags;
    if(req->ac.fin) { strcpy(p, ",fin"); p+=4; }
    if(req->ac.fir) { strcpy(p, ",fir"); p+=4; }
    if(req->ac.con) { strcpy(p, ",con"); p+=4; }
    if(req->ac.uns) { strcpy(p, ",uns"); p+=4; }
    if(p > flags) {
        flags[0] = '(';
        *p++ = ')';
        *p++ = ' ';
    }
    *p = '\0';

    // authdata string
    char *auth = "";
    if(req->auth) {
        auth = " [auth]";   // XXX
    }

    // object data
    size = 1;
    odata = malloc(size);
    if(!odata) goto err;
    odata[0] = '\0';
    for(size_t i=0; i<req->nblocks; i++) {
        blk = dnp3_format_oblock(req->odata[i]);
        if(!blk) goto err;

        size += 3 + strlen(blk);  // " {$blk}"

        p = realloc(odata, size);
        if(!p) goto err;
        odata = p;

        strcat(odata, " {");
        strcat(odata, blk);
        strcat(odata, "}");

        free(blk);
        blk = NULL;
    }

    char *name = funcname[req->fc];
    size = 6 + strlen(flags) + strlen(name) + strlen(odata) + strlen(auth);
    res = malloc(size);
    if(!res) goto err;

    size_t n = snprintf(res, size, "[%d] %s%s%s%s",
                        req->ac.seq, flags, name, odata, auth);
    if(n < 0) goto err;

    return res;

err:
    if(blk) free(blk);
    if(odata) free(odata);
    if(res) free(res);
    return NULL;
}



// XXX move these tables someplace sensible

// response function codes and associated (possible) object types
//
// this table is an inversion of table 3 ("Object definition summary")
// of AN2013-004b.
//
// rspfc | reqfc | grp(/var)...
//-------------------------------------------------------------------------
//   129     ???   0,1-4*,10*,11*,13*,20-23*,30-34*,40-43*,50/1,50/4,51,
//                 70/2,70/4-7,80,81,82,83,85*,86,87,88*,91,101,102,110,
//                 111,113*,120/3,120/9,121*,122*
//                 XXX can also include 120/1 (authentication challenge)!
//   130     -/-   2*,4*,11*,13*,22*,23*,32*,33*,42*,43*,51,70/4-7,82,83/1,
//                 85*,88*,111,113*,120/3,120/9,122*
//   131     ???   120/1-2,120/5,120/7,120/12,120/15
//                 * excluding variation 0 ("any")

// request function codes and associated (possible) object types
//
// this table is an inversion of table 3 ("Object definition summary")
// of AN2013-004b.
//
// reqfc | grp(/var)...
//----------------------------------------------------------------------
//     0   (120/3,120/9) // XXX missing from AN2013-004b (cf. IEEE 1815-2012 7.5.1.4 Figure 7-16)
//     1   0-4,10,11,13,20-23,30-34,40,42,43,50/1,50/4,60,70/5-6,
//         80,81,83,85,86/1-3,87,88,101,102,110,111,113/0,121,122,
//         (120/3,120/9)
//     2   0/240,0/245-247,10/1,34/1-3,50/1,50/3-4,70/5,80,85/0,86/1,
//         86/3,87/1,101,102,110,112, (120/3,120/9)
//     3   12/1-3,41/1-4,87/1, (120/3,120/9)
//     4   12/1-3,41/1-4,87/1, (120/3,120/9)
//     5   12/1-3,41/1-4,87/1, (120/3,120/9)
//     6   12/1-3,41/1-4,87/1, (120/3,120/9)
//     7   20/0,30/0, (120/3,120/9)
//     8   20/0,30/0, (120/3,120/9)
//     9   20/0, (120/3,120/9)
//    10   20/0, (120/3,120/9)
//    11   20/0,30/0,50/2, (120/3,120/9)
//    12   20/0,30/0(?),50/2, (120/3,120/9) // XXX 30/0 missing from AN2013-004b?!
//    13   (120/3,120/9)
//    14   (120/3,120/9)
//    15   (120/3,120/9)
//    16   90/1, (120/3,120/9)
//    17   90/1, (120/3,120/9)
//    18   90/1, (120/3,120/9)
//    19   (120/3,120/9)
//    20   60/2-4, (120/3,120/9)
//    21   60/2-4, (120/3,120/9)
//    22   1/0,3/0,10/0,12/0,20/0,21/0,30/0,31/0,40/0,41/0,60,86/0,110,
//         113/0,120/0,121/0, (120/3,120/9)
//    23   (120/3,120/9)
//    24   (120/3,120/9)
//    25   70/3, (120/3,120/9)
//    26   70/3, (120/3,120/9)
//    27   70/3, (120/3,120/9)
//    28   70/7, (120/3,120/9)
//    29   70/2, (120/3,120/9)
//    30   70/4, (120/3,120/9)
//    31   70/8, (120/3,120/9)
//    32   120/1-2,120/4,120/6,120/8,120/10-11,120/13-15
//    33   120/7
//
// as seen above, all request function codes except 32 (AUTHENTICATE_REQ) and
// 33 (AUTH_REQ_NO_ACK) can include the "aggressive mode authentication"
// objects g120v3 and g120v9. the combinator 'ama' defined below adds the
// authentication object parsing.
