// application layer

#include <dnp3.h>
#include <hammer/glue.h>
#include "hammer.h" // XXX placeholder for extensions
#include <string.h>
#include <stdlib.h> // malloc
#include <inttypes.h> // PRIu32 etc.
#include "app.h"
#include "obj/binary.h"
#include "g13_binoutcmdev.h"
#include "obj/counter.h"
#include "g120_auth.h"
#include "util.h"


/// APPLICATION HEADER ///


/// AGGRESSIVE-MODE AUTHENTICATION ///

static HParsedToken *act_with_ama(const HParseResult *p, void *env)
{
    DNP3_Fragment *frag = H_ALLOC(DNP3_Fragment);
    DNP3_AuthData *a = H_ALLOC(DNP3_AuthData);

    // XXX this is wrong; the incoming objects are raw, not yet a DNP3_Fragment
    //     leave all this to act_fragment!?

    // input is a sequence: (auth_aggr, fragment, auth_mac)
    *frag = *H_FIELD(DNP3_Fragment, 1);

    // XXX fill AuthData from H_FIELD(..., 0) and H_FIELD(..., 2)
    // a->xxx = H_FIELD(..., 1)->xxx;
    // a->mac = H_FIELD(..., 2);
    frag->auth = a;

    return H_MAKE(DNP3_Fragment, frag);
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
HParser *dnp3_p_app_response;

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

static HParser *get_rsc;
static HParser *get_base;

// helper
static HParser *int_exact(HParser *p, uint64_t x)
{
    return h_int_range(p, x, x);
}

// prefix code
static HParser *withpc(uint8_t x, HParser *p)
{
    H_RULE(pc_, h_bits(3, false));
    H_RULE(pc,  bit_big_endian(h_right(dnp3_p_reserved(1), pc_)));

    return h_sequence(int_exact(pc,x), p, NULL);
}
static HParser *noprefix(HParser *p)
{
    return withpc(0, p);
}

// range specifier code
static HParser *rsc(uint8_t x)
{
    HParser *p = int_exact(h_bits(4, 0), x);

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

static HParser *oblock_range__(HParser *p)
{
    H_RULE(range,   h_choice(range_index, range_addr, NULL));
        // XXX are address ranges really allowed with all types of objects or
        //     only where the spec actually says so (g102, g110)?
    H_RULE(objs,    h_action(h_length_value(range, p),
                             act_objects_only, NULL));

    // this variant does not attach the prefix code, yet, so we can stick an
    // endianness combinator in between for the packed variations (see below)
    return objs;
}

static HParser *oblock_range_(HParser *p)
{
    return noprefix(oblock_range__(p));
}

static HParser *oblock_packed_(HParser *p)
{
    H_RULE(objs,        oblock_range__(p));
    H_RULE(objs_pad,    h_left(objs, dnp3_p_pad));

    return noprefix(objs_pad);
}

static HParser *oblock_index_(HParser *p)
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

    H_RULE(rblock_index, oblock_index_(NULL));
    rblock_ = h_choice(ohdr_irange, ohdr_arange, ohdr_all, ohdr_count,
                       rblock_index, NULL);

    // parsers to fetch the saved range values (used in block())
    get_rsc =   h_get_value("rsc");
    get_base =  h_optional(h_get_value("range_base"));
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
    if(H_ISERR(H_INDEX_TOKEN(p->ast, 2)->token_type))
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
        // tok corresponds to the block_ argument of block()
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
    // the error propagation dance
    // we want a parse failure in group and variation to lead to failure and
    // one in the rest to yield PARAM_ERROR.
    H_RULE (rest,   h_sequence(block_, dnp3_p_pad, get_rsc, get_base, NULL));
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

HParser *dnp3_p_oblock(DNP3_Group g, DNP3_Variation v, HParser *obj)
{
    H_RULE(oblock_, h_choice(oblock_index_(obj),
                             oblock_range_(obj), NULL));

    return block(group(g), variation(v), oblock_);
}

HParser *dnp3_p_oblock_packed(DNP3_Group g, DNP3_Variation v, HParser *obj)
{
    return block(group(g), variation(v), oblock_packed_(obj));
}

HParser *dnp3_p_oblock_vf(DNP3_Group g, DNP3_Variation v, HParser *(*obj)(size_t))
{
    H_RULE(oblock_, oblock_vf_(range_vfcount, obj));

    return block(group(g), variation(v), oblock_);
}

static void init_odata(void)
{
    H_RULE(confirm, h_epsilon_p());

    // read request object blocks:
    //   may use variation 0 (any)
    //   may use group 60 (event class data)
    //   may use range specifier 0x6 (all objects)

    // device attributes
    //H_RULE(rblock_attr,     dnp3_p_attr_rblock);

    // binary inputs
    H_RULE(rblock_binin,    h_choice(dnp3_p_binin_rblock,
                                     dnp3_p_bininev_rblock,
                                     dnp3_p_dblbitin_rblock,
                                     dnp3_p_dblbitinev_rblock, NULL));
    H_RULE(oblock_binin,    h_choice(dnp3_p_binin_oblock,
                                     dnp3_p_bininev_oblock,
                                     dnp3_p_dblbitin_oblock,
                                     dnp3_p_dblbitinev_oblock, NULL));

    // binary outputs
    H_RULE(rblock_binout,   h_choice(dnp3_p_binout_rblock,
                                     dnp3_p_binoutev_rblock,
                                     dnp3_p_binoutcmdev_rblock, NULL));
    H_RULE(oblock_binout,   h_choice(dnp3_p_binout_oblock,
                                     dnp3_p_binoutev_oblock,
                                     dnp3_p_binoutcmdev_oblock, NULL));

    // counters
    H_RULE(rblock_ctr,      h_choice(dnp3_p_ctr_rblock,
                                     dnp3_p_ctrev_rblock,
                                     dnp3_p_frozenctr_rblock,
                                     dnp3_p_frozenctrev_rblock, NULL));
    H_RULE(oblock_ctr,      h_choice(dnp3_p_ctr_oblock,
                                     dnp3_p_ctrev_oblock,
                                     dnp3_p_frozenctr_oblock,
                                     dnp3_p_frozenctrev_oblock, NULL));

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

    H_RULE(read_oblock,     dnp3_p_objchoice(//rblock_attr,
                                             rblock_binin,
                                             rblock_binout,
                                             rblock_ctr,
                                             NULL));
    H_RULE(read,            dnp3_p_many(read_oblock));
    // XXX NB parsing pseudocode in AN2012-004b does NOT work for READ requests.
    //     it misses the case that a function code requires object headers
    //     but no objects. never mind that it might require an object with some
    //     variations but not others (e.g. g70v5 file transmission).

    H_RULE(wblock_binout,   dnp3_p_binout_wblock);
    H_RULE(write_oblock,    dnp3_p_objchoice(//wblock_attr,
                                             wblock_binout,
                                             NULL));
    H_RULE(write,           dnp3_p_many(write_oblock));

    H_RULE(rsp_oblock,      dnp3_p_objchoice(//oblock_attr,
                                             oblock_binin,
                                             oblock_binout,
                                             oblock_ctr,
                                             NULL));
    H_RULE(response,        dnp3_p_many(rsp_oblock));

    H_RULE(unsolicited,     h_epsilon_p()); // XXX


    odata[DNP3_CONFIRM] = ama(confirm);
    odata[DNP3_READ]    = ama(read);
    odata[DNP3_WRITE]   = ama(write);

        // read_rsp_object:
        //   may not use variation 0
        //   may not use group 60
        //   may not use range specifier 0x6
    odata[DNP3_RESPONSE] = ama(response);    // XXX ? or depend on req. fc?!
    odata[DNP3_UNSOLICITED_RESPONSE] = ama(unsolicited);

    //odata[DNP3_AUTHENTICATE_REQ]    = authenticate_req;
    //odata[DNP3_AUTH_REQ_NO_ACK]     = auth_req_no_ack;
}


/// APPLICATION LAYER FRAGMENTS ///

// combine header, auth, and object data into final DNP3_Fragment
static HParsedToken *act_fragment(const HParseResult *p, void *user)
{
    const HParsedToken *hdr = user;
    // hdr = (ac, fc)
    //     | (ac, fc, iin)

    // extract the application header
    DNP3_Fragment *frag = H_ALLOC(DNP3_Fragment);
    frag->ac = *H_INDEX(DNP3_AppControl, hdr, 0);
    frag->fc = H_INDEX_UINT(hdr, 1);
    if(h_seq_len(hdr) > 2)
        frag->iin = *H_INDEX(DNP3_IntIndications, hdr, 2);

    // propagate TT_ERR on objects
    if(p->ast && H_ISERR(p->ast->token_type)) {
        // we use (XXX abuse?) the user field on our TT_ERR token to report the
        // application header (as a DNP3_Fragment structure without objects),
        // so that an outstation can generate a correct response to requests.
        return h_make_err(p->arena, p->ast->token_type, frag);
    }

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
        frag->auth = H_INDEX(DNP3_AuthData, od, 0);
        od = H_INDEX_TOKEN(od, 1);
    }

    if(od == NULL) {
        // empty case (no odata)
        frag->odata = NULL;
    } else if(od->token_type == TT_SEQUENCE) {
        // extract object blocks
        size_t n = od->seq->used;
        frag->nblocks = n;
        frag->odata = h_arena_malloc(p->arena, sizeof(DNP3_ObjectBlock *) * n);
        for(size_t i=0; i<frag->nblocks; i++) {
            frag->odata[i] = H_INDEX(DNP3_ObjectBlock, od, i);
        }
    } else {
        // single-oblock case
        frag->nblocks = 1;
        frag->odata = H_ALLOC(DNP3_ObjectBlock *);
        frag->odata[0] = H_CAST(DNP3_ObjectBlock, od);
    }

    return H_MAKE(DNP3_Fragment, frag);
}

static HParsedToken *act_fragment_errfc(const HParseResult *p, void *user)
{
    HParsedToken *ac = user;

    // return a DNP3_Fragment containing the parsed application control octet
    DNP3_Fragment *frag = H_ALLOC(DNP3_Fragment);
    frag->ac = *H_CAST(DNP3_AppControl, ac);
    frag->fc = p->ast->uint;

    return h_make_err(p->arena, p->ast->token_type, frag);
}

// parse the rest of a fragment, after the application header
static HParser *f_fragment(const HParsedToken *hdr, void *env)
{
    // propagate TT_ERR on function code
    HParsedToken *fc_ = H_INDEX_TOKEN(hdr, 1);
    if(H_ISERR(fc_->token_type)) {
        HParsedToken *ac = H_INDEX_TOKEN(hdr, 0);
        return h_action(h_unit(fc_), act_fragment_errfc, (void *)ac);
    }

    int fc = H_CAST_UINT(fc_);

    // basic object data parser
    HParser *p = odata[fc];
    assert(p != NULL);

    // odata must always parse the entire rest of the fragment
    p = dnp3_p_packet(p);

    // any unspecific parse failure on odata should yield PARAM_ERROR
    p = h_choice(p, h_error(ERR_PARAM_ERROR), NULL);

    return h_action(p, act_fragment, (void *)hdr);
}

static HParsedToken *act_iin(const HParseResult *p, void *user)
{
    DNP3_IntIndications *iin = H_ALLOC(DNP3_IntIndications);

    iin->broadcast  = H_FIELD_UINT(0);
    iin->class1     = H_FIELD_UINT(1);
    iin->class2     = H_FIELD_UINT(2);
    iin->class3     = H_FIELD_UINT(3);
    iin->need_time  = H_FIELD_UINT(4);
    iin->local_ctrl = H_FIELD_UINT(5);
    iin->device_trouble = H_FIELD_UINT(6);
    iin->device_restart = H_FIELD_UINT(7);
    iin->func_not_supp  = H_FIELD_UINT(8);
    iin->obj_unknown    = H_FIELD_UINT(9);
    iin->param_error    = H_FIELD_UINT(10);
    iin->eventbuf_overflow  = H_FIELD_UINT(11);
    iin->already_executing  = H_FIELD_UINT(12);
    iin->config_corrupt     = H_FIELD_UINT(13);
    // 14 bits total

    return H_MAKE(DNP3_IntIndications, iin);
}

static HParsedToken *act_ac(const HParseResult *p, void *user)
{
    DNP3_AppControl *ac = H_ALLOC(DNP3_AppControl);

    ac->seq = H_FIELD_UINT(0);
    ac->uns = H_FIELD_UINT(1, 0);
    ac->con = H_FIELD_UINT(1, 1);
    ac->fin = H_FIELD_UINT(1, 2);
    ac->fir = H_FIELD_UINT(1, 3);

    return H_MAKE(DNP3_AppControl, ac);
}

#define act_reqac act_ac
#define act_conac act_ac
#define act_unsac act_ac
#define act_rspac act_ac

static HParsedToken *act_errfc(const HParseResult *p, void *user)
{
    return h_make_err_uint(p->arena, ERR_FUNC_NOT_SUPP, H_CAST_UINT(p->ast));
}

#define act_ereqfc act_errfc
#define act_erspfc act_errfc

void dnp3_p_init_app(void)
{
    // initialize object block and associated parsers/combinators
    init_oblock();

    // initialize object parsers
    dnp3_p_init_binary();
    dnp3_p_init_g13_binoutcmdev();
    dnp3_p_init_counter();

    // initialize request-specific "object data" parsers
    init_odata();

    H_RULE (bit,    h_bits(1, false));
    H_RULE (zro,    int_exact(bit, 0));
    H_RULE (one,    int_exact(bit, 1));
    H_RULE (ign,    bit); // to be ignored

                          /* --- uns,con,fin,fir --- */
    H_RULE (conflags, h_sequence(bit,zro,one,one, NULL));   // CONFIRM
    H_RULE (reqflags, h_sequence(zro,zro,one,one, NULL));   // always fin,fir!
    H_RULE (unsflags, h_sequence(one,one,ign,ign, NULL));   // unsolicited
    H_RULE (rspflags, h_sequence(zro,bit,bit,bit, NULL));

    H_RULE (seqno,  h_bits(4, false));
    H_ARULE(conac,  h_sequence(seqno, conflags, NULL));
    H_ARULE(reqac,  h_sequence(seqno, reqflags, NULL));
    H_ARULE(unsac,  h_sequence(seqno, unsflags, NULL));
    H_ARULE(rspac,  h_sequence(seqno, rspflags, NULL));
    H_ARULE(iin,    h_left(h_repeat_n(bit, 14), dnp3_p_reserved(2)));

    H_RULE (anyreqac, h_choice(conac, reqac, NULL));
    H_RULE (anyrspac, h_choice(unsac, rspac, NULL));

    H_RULE (fc,     h_uint8());
    H_RULE (fc_rsp, int_exact(fc, DNP3_RESPONSE));
    H_RULE (fc_ur,  int_exact(fc, DNP3_UNSOLICITED_RESPONSE));
    H_RULE (fc_ar,  int_exact(fc, DNP3_AUTHENTICATE_RESP));

    H_RULE (confc,  int_exact(fc, DNP3_CONFIRM));
    H_RULE (reqfc,  h_int_range(fc, 0x01, 0x21));
    H_RULE (unsfc,  h_choice(fc_ur, fc_ar, NULL));
    H_RULE (rspfc,  h_choice(fc_rsp, fc_ar, NULL));

    H_RULE (anyreqfc,   h_choice(confc, reqfc, NULL));
    H_RULE (anyrspfc,   h_choice(unsfc, rspfc, NULL));

    H_ARULE(ereqfc,     h_right(h_and(h_not(anyreqfc)), fc));
    H_ARULE(erspfc,     h_right(h_and(h_not(anyrspfc)), fc));

    H_RULE (req_header, h_choice(h_sequence(conac, confc, NULL),
                                 h_sequence(reqac, reqfc, NULL),
                                 h_sequence(anyreqac, ereqfc, NULL), NULL));
    H_RULE (rsp_header, h_choice(h_sequence(unsac, unsfc, iin, NULL),
                                 h_sequence(rspac, rspfc, iin, NULL),
                                 h_sequence(anyrspac, erspfc, iin, NULL), NULL));

    H_RULE (request,    h_bind(req_header, f_fragment, NULL));
    H_RULE (response,   h_bind(rsp_header, f_fragment, NULL));

    dnp3_p_app_request  = little_endian(request);
    dnp3_p_app_response = little_endian(response);
}


/// human-readable output formatting ///
// XXX should this move into its own file?

static char *funcnames[] = {
    // requests 0x00 - 0x21
    "CONFIRM", "READ", "WRITE", "SELECT", "OPERATE", "DIRECT_OPERATE",
    "DIRECT_OPERATE_NR", "IMMED_FREEZE", "IMMED_FREEZE_NR", "FREEZE_CLEAR",
    "FREEZE_CLEAR_NR", "FREEZE_AT_TIME", "FREEZE_AT_TIME_NR", "COLD_RESTART",
    "WARM_RESTART", "INITIALIZE_DATA", "INITIALIZE_APPL", "START_APPL",
    "STOP_APPL", "SAVE_CONFIG", "ENABLE_UNSOLICITED", "DISABLE_UNSOLICITED",
    "ASSIGN_CLASS", "DELAY_MEASURE", "RECORD_CURRENT_TIME", "OPEN_FILE",
    "CLOSE_FILE", "DELETE_FILE", "GET_FILE_INFO", "AUTHENTICATE_FILE",
    "ABORT_FILE", "ACTIVATE_CONFIG", "AUTHENTICATE_REQ", "AUTH_REQ_NO_ACK",

    // 0x22 - 0x80
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,     // 0x28
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,     // 0x30
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,     // 0x40
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,     // 0x60
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,     // 0x80

    // responses 0x81-0x83
    "RESPONSE", "UNSOLICITED_RESPONSE", "AUTHENTICATE_RESP"
    };

int appendf(char **s, size_t *size, const char *fmt, ...)
{
    va_list args;
    size_t len;
    int n;
    char *p;

    assert(s != NULL);
    if(*s == NULL) {
        *s = malloc(n = 10);
        if(!*s) return -1;
        *s[0] = '\0';
        *size = n;
    }

    len = strlen(*s);
    while(1) {
        size_t left = *size - len;
        va_start(args, fmt);
        n = vsnprintf(*s + len, left, fmt, args);
        va_end(args);
        if(n < 0)
            return -1;
        if(n < left)
            break;

        // need more space
        n = len + n + 1;
        p = realloc(*s, n);
        if(!p)
            return -1;
        *size = n;
        *s = p;
    }

    return 0;
}

static const char dblbit_sym[] = "~01-";

static char *format_flags(DNP3_Flags flags)
{
    char *res = NULL;
    size_t n;

    if(flags.online)            appendf(&res, &n, ",online");
    if(flags.restart)           appendf(&res, &n, ",restart");
    if(flags.comm_lost)         appendf(&res, &n, ",comm_lost");
    if(flags.remote_forced)     appendf(&res, &n, ",remote_forced");
    if(flags.local_forced)      appendf(&res, &n, ",local_forced");
    if(flags.chatter_filter)    appendf(&res, &n, ",chatter_filter");
    //if(flags.rollover)          appendf(&res, &n, ",rollover");
    if(flags.discontinuity)     appendf(&res, &n, ",discontinuity");

    return res;
}

static int append_flags(char **res, size_t *size, DNP3_Flags flags)
{
    char *s = format_flags(flags);
    int x;

    if(s) {
        x = appendf(res, size, "(%s)", s+1);
        free(s);
    } else {
        x = 0;
    }

    return x;
}

static int append_bin_flags(char **res, size_t *size, DNP3_Flags flags)
{
    if(append_flags(res, size, flags) < 0) return -1;
    return appendf(res, size, "%d", (int)flags.state);
}

static int append_dblbit_flags(char **res, size_t *size, DNP3_Flags flags)
{
    if(append_flags(res, size, flags) < 0) return -1;
    return appendf(res, size, "%c", (int)dblbit_sym[flags.state]);
}

static int append_time(char **res, size_t *size, uint64_t time, bool relative)
{
    uint64_t s  = time / 1000;
    uint64_t ms = time % 1000;
    const char *fmt;

    if(relative)
        fmt = ms ? "@+%"PRIu64".%.3"PRIu64 : "@+%"PRIu64;
    else
        fmt = ms ? "@%"PRIu64".%.3"PRIu64 : "@%"PRIu64;
    return appendf(res, size, fmt, s, ms);
}

#define append_abstime(res, size, time) append_time(res, size, time, false)
#define append_reltime(res, size, time) append_time(res, size, time, true)

char *dnp3_format_object(DNP3_Group g, DNP3_Variation v, const DNP3_Object o)
{
    size_t size;
    char *res = NULL;
    size_t fsize;

    switch(g << 8 | v) {
    case GV(BININ, PACKED):
    case GV(BINOUT, PACKED):
        appendf(&res, &size, "%d", (int)o.bit);
        break;
    case GV(BININ, FLAGS):
    case GV(BINOUT, FLAGS):
    case GV(BININEV, NOTIME):
    case GV(BINOUTEV, NOTIME):
        append_bin_flags(&res, &size, o.flags);
        break;
    case GV(BININEV, ABSTIME):
    case GV(BINOUTEV, ABSTIME):
        append_bin_flags(&res, &size, o.timed.flags);
        append_abstime(&res, &size, o.timed.abstime);
        break;
    case GV(BININEV, RELTIME):
        append_bin_flags(&res, &size, o.timed.flags);
        append_reltime(&res, &size, o.timed.reltime);
        break;
    case GV(DBLBITIN, PACKED):
        appendf(&res, &size, "%c", (int)dblbit_sym[o.dblbit]);
        break;
    case GV(DBLBITIN, FLAGS):
    case GV(DBLBITINEV, NOTIME):
        append_dblbit_flags(&res, &size, o.flags);
        break;
    case GV(DBLBITINEV, ABSTIME):
        append_dblbit_flags(&res, &size, o.timed.flags);
        append_abstime(&res, &size, o.timed.abstime);
        break;
    case GV(DBLBITINEV, RELTIME):
        append_dblbit_flags(&res, &size, o.timed.flags);
        append_reltime(&res, &size, o.timed.reltime);
        break;
    case GV(BINOUTCMDEV, NOTIME):
        appendf(&res, &size, "(cs=%d,status=%d)",
                (int)o.cmdev.cs, (int)o.cmdev.status);
        break;
    case GV(BINOUTCMDEV, ABSTIME):
        appendf(&res, &size, "(cs=%d,status=%d)",
                (int)o.timed.cmdev.cs, (int)o.timed.cmdev.status);
        append_abstime(&res, &size, o.timed.abstime);
        break;
    case GV(CTR, 32BIT_FLAG):
    case GV(CTR, 16BIT_FLAG):
    case GV(CTREV, 16BIT_FLAG):
    case GV(CTREV, 32BIT_FLAG):
    case GV(FROZENCTR, 32BIT_FLAG):
    case GV(FROZENCTR, 16BIT_FLAG):
    case GV(FROZENCTREV, 32BIT_FLAG):
    case GV(FROZENCTREV, 16BIT_FLAG):
        append_flags(&res, &size, o.ctr.flags);
        // fall through to next case to append counter value
    case GV(CTR, 32BIT_NOFLAG):
    case GV(CTR, 16BIT_NOFLAG):
    case GV(FROZENCTR, 32BIT_NOFLAG):
    case GV(FROZENCTR, 16BIT_NOFLAG):
        appendf(&res, &size, "%"PRIu64, o.ctr.value);
        break;
    case GV(CTREV, 16BIT_FLAG_TIME):
    case GV(CTREV, 32BIT_FLAG_TIME):
    case GV(FROZENCTR, 32BIT_FLAG_TIME):
    case GV(FROZENCTR, 16BIT_FLAG_TIME):
    case GV(FROZENCTREV, 32BIT_FLAG_TIME):
    case GV(FROZENCTREV, 16BIT_FLAG_TIME):
        append_flags(&res, &size, o.timed.ctr.flags);
        appendf(&res, &size, "%"PRIu64, o.timed.ctr.value);
        append_abstime(&res, &size, o.timed.abstime);
        break;
    }

    if(!res)
        appendf(&res, &size, "?");

    return res;
}

char *dnp3_format_oblock(const DNP3_ObjectBlock *ob)
{
    size_t size;
    char *res = NULL;
    const char *sep = ob->objects ? ":" : "";
    int x;

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
                DNP3_Object o = ob->objects[i];
                char *s = dnp3_format_object(ob->group, ob->variation, o);
                x = appendf(&res, &size, "%s", s);
                free(s);
                if(x<0) goto err;
            }
        }
    }

    return res;

err:
    if(res) free(res);
    return NULL;
}

char *dnp3_format_fragment(const DNP3_Fragment *frag)
{
    char *res = NULL;
    size_t size;
    int x;

    // flags string
    char flags[20]; // need 4*3(names)+3(seps)+2(parens)+1(space)+1(null)
    char *p = flags;
    if(frag->ac.fin) { strcpy(p, ",fin"); p+=4; }
    if(frag->ac.fir) { strcpy(p, ",fir"); p+=4; }
    if(frag->ac.con) { strcpy(p, ",con"); p+=4; }
    if(frag->ac.uns) { strcpy(p, ",uns"); p+=4; }
    if(p > flags) {
        flags[0] = '(';
        *p++ = ')';
        *p++ = ' ';
    }
    *p = '\0';

    // begin assembly of result string
    x = appendf(&res, &size, "[%d] %s", (int)frag->ac.seq, flags);
    if(x<0) goto err;

    // function name
    char *name = NULL;
    if(frag->fc < sizeof(funcnames) / sizeof(char *))
        name = funcnames[frag->fc];
    if(name)
        x = appendf(&res, &size, "%s", name);
    else
        x = appendf(&res, &size, "0x%.2X", (unsigned int)frag->fc);
    if(x<0) goto err;

    // add internal indications
    char *iin = NULL;
    size_t iinsize;
    #define APPEND_IIN(FLAG) if(frag->iin.FLAG) appendf(&iin, &iinsize, "," #FLAG)
    APPEND_IIN(broadcast);
    APPEND_IIN(class1);
    APPEND_IIN(class2);
    APPEND_IIN(class3);
    APPEND_IIN(need_time);
    APPEND_IIN(local_ctrl);
    APPEND_IIN(device_trouble);
    APPEND_IIN(device_restart);
    APPEND_IIN(func_not_supp);
    APPEND_IIN(obj_unknown);
    APPEND_IIN(param_error);
    APPEND_IIN(eventbuf_overflow);
    APPEND_IIN(already_executing);
    APPEND_IIN(config_corrupt);
    #undef APPEND_IIN
    if(iin) {
        x = appendf(&res, &size, " (%s)", iin+1); // +1 to skip the leading ','
        free(iin);
        if(x<0) goto err;
    }

    // add object data
    for(size_t i=0; i<frag->nblocks; i++) {
        char *blk = dnp3_format_oblock(frag->odata[i]);
        if(!blk) goto err;

        x = appendf(&res, &size, " {%s}", blk);
        free(blk);
        if(x<0) goto err;
    }

    // add authdata
    if(frag->auth) {
        x = appendf(&res, &size, " [auth]");    // XXX
        if(x<0) goto err;
    }

    return res;

err:
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
