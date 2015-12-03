
#include <dnp3hammer.h>

#include <hammer/hammer.h>
#include <hammer/glue.h>
#include <stdlib.h>     // malloc
#include "hammer.h"
#include "app.h"
#include "util.h"


static HParser *range_index;
static HParser *range_addr;
static HParser *range_none;
static HParser *range_count;
static HParser *range_max;
static HParser *range_count1;
static HParser *range_vfcount;
static HParser *range_vfcount1;

static HParser *ohdr_irange;
static HParser *ohdr_arange;
static HParser *ohdr_all;
static HParser *ohdr_count;
static HParser *ohdr_max;
static HParser *ohdr_count1;

static HParser *rblock_;
static HParser *rblock_max;

static HParser *get_rsc;
static HParser *get_base;


// prefix code
static HParser *withpc(uint8_t x, HParser *p)
{
    H_RULE(pc_, h_bits(3, false));
    H_RULE(pc,  bit_big_endian(h_right(dnp3_p_reserved(1), pc_)));

    return h_sequence(dnp3_p_int_exact(pc,x), p, NULL);
}
static HParser *noprefix(HParser *p)
{
    return withpc(0, p);
}

// range specifier code
static HParser *rsc(uint8_t x)
{
    HParser *p = dnp3_p_int_exact(h_bits(4, 0), x);

    // funnel the rsc out via h_put_value so we can find and save it in the
    // DNP3_ObjectBlock struct later
    return h_put_value(p, "rsc");
}

// range fields giving an actual range
static bool validate_range(HParseResult *p, void *user)
{
    // p->ast = (start, stop)
    uint64_t start = H_FIELD_UINT(0);
    uint64_t stop  = H_FIELD_UINT(1);

    // validate that start <= stop
    // validate that count (stop - start + 1) will fit in size_t
    return (start <= stop && stop-start < SIZE_MAX);
}
static HParsedToken *act_range(const HParseResult *p, void *user)
{
    // p->ast = (start, stop)
    uint64_t start = H_FIELD_UINT(0);
    uint64_t stop  = H_FIELD_UINT(1);

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
static bool validate_count(HParseResult *p, void *user)
{
    return (H_CAST_UINT(p->ast) > 0);
}
static HParser *count(uint8_t x, HParser *p)
{
    return h_right(rsc(x), h_attr_bool(p, validate_count, NULL));
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

static HParser *k_bindvf(HAllocator *mm__, const HParsedToken *n, void *user)
{
    HParser *(*q)(HAllocator *, size_t) = user;

    return q(mm__, H_CAST_UINT(n));
}
static HParser *prefixed_size(HParser *vfcnt, HParser *p, HParser *(*q)(HAllocator *, size_t))
{
    return h_action(h_length_value(vfcnt, h_bind(p, k_bindvf, q)),
                    act_objects_only, NULL);
}

static HParser *oblock_range_(HParser *p)
{
    H_RULE(range,   h_choice(range_index, range_addr, NULL));
        // XXX are address ranges really allowed with all types of objects or
        //     only where the spec actually says so (g102, g110)?
    H_RULE(objs,    h_action(h_length_value(range, p),
                             act_objects_only, NULL));

    return noprefix(objs);
}

static HParser *oblock_index_(HParser *p)
{
    return h_choice(withpc(1, prefixed_index(h_uint8(), p)), 
                    withpc(2, prefixed_index(h_uint16(), p)),
                    withpc(3, prefixed_index(h_uint32(), p)), NULL);
}

static HParser *oblock_vf_(HParser *cnt, HParser *(*p)(HAllocator *mm__, size_t))
{
    return h_choice(withpc(4, prefixed_size(cnt, h_uint8(), p)), 
                    withpc(5, prefixed_size(cnt, h_uint16(), p)),
                    withpc(6, prefixed_size(cnt, h_uint32(), p)), NULL);
}

void init_oblock(void)
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
    range_max =    h_choice(count(0x7, h_uint8()),
                            count(0x8, h_uint16()), NULL);
    range_count1 =          count(0x7, h_ch(1)),    // a single object
                               // 0xA = reserved
    range_vfcount =         count(0xB, h_uint8());  // count of var-size objects
    range_vfcount1 =        count(0xB, h_ch(1));    // a single var-size object

    ohdr_irange = noprefix(range_index);
    ohdr_arange = noprefix(range_addr);
    ohdr_all    = noprefix(range_none);
    ohdr_count  = noprefix(range_count);
    ohdr_max    = noprefix(range_max);
    ohdr_count1 = noprefix(range_count1);

    H_RULE(rblock_index, oblock_index_(NULL));
    rblock_ = h_choice(ohdr_irange, ohdr_arange, ohdr_all, ohdr_count,
                       rblock_index, NULL);

    rblock_max = h_choice(ohdr_all, ohdr_count, NULL);

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
    H_RULE (e_rest, h_choice(rest, dnp3_p_err_param_error, NULL));
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
    // XXX this array is never freed (but no parsers ever are)
    // XXX ensure we call parser-allocating functions only once during init
    vs = malloc((n+2) * sizeof(HParser *));
    vs[0] = variation(DNP3_VARIATION_ANY);
    va_start(args, g);
    for(i=1; i<=n; i++)
        vs[i] = variation(va_arg(args, DNP3_Variation));
    va_end(args);
    vs[i] = NULL;

    return block(group(g), h_choice__a((void **)vs), rblock_);
}

HParser *dnp3_p_specific_rblock(DNP3_Group g, DNP3_Variation v)
{
    return block(group(g), variation(v), rblock_);
}

HParser *dnp3_p_rblock_all(DNP3_Group g, DNP3_Variation v)
{
    return block(group(g), variation(v), ohdr_all);
}

HParser *dnp3_p_rblock_max(DNP3_Group g, DNP3_Variation v)
{
    return block(group(g), variation(v), rblock_max);
}

HParser *dnp3_p_single(DNP3_Group g, DNP3_Variation v, HParser *obj)
{
    H_RULE(objs_, h_length_value(range_count1, obj));
    H_RULE(objs,  h_action(objs_, act_objects_only, NULL));

    return block(group(g), variation(v), noprefix(objs));
}

HParser *dnp3_p_single_rblock(DNP3_Group g, DNP3_Variation v)
{
    return block(group(g), variation(v), ohdr_count1);
}

HParser *dnp3_p_single_vf(DNP3_Group g, DNP3_Variation v, HParser *(*obj)(HAllocator *mm__, size_t))
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
    return block(group(g), variation(v), oblock_range_(obj));
}

HParser *dnp3_p_oblock_vf(DNP3_Group g, DNP3_Variation v, HParser *(*obj)(HAllocator *mm__, size_t))
{
    H_RULE(oblock_, oblock_vf_(range_vfcount, obj));

    return block(group(g), variation(v), oblock_);
}
