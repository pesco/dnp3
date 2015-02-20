// application layer

#include <dnp3.h>
#include <hammer/glue.h>
#include "hammer.h" // XXX placeholder for extensions
#include "obj/binary.h"
#include "obj/binoutcmd.h"
#include "obj/counter.h"
#include "obj/analog.h"
#include "obj/time.h"
#include "obj/class.h"
#include "obj/iin.h"
#include "g120_auth.h"
#include "util.h"

#include "app.h"


HParser *dnp3_p_app_request;
HParser *dnp3_p_app_response;


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

// response function codes and associated (possible) object types
//
// this table is an inversion of table 3 ("Object definition summary")
// of AN2013-004b.
//
// rspfc | reqfc | grp(/var)...
//-------------------------------------------------------------------------
//   129     ???   0,1-4*,10*,11*,12*,13*,20-23*,30-34*,40-43*,50/1,50/4,
//                 51,52,70/2,70/4-7,80,81,82,83,85*,86,87,88*,91,101,102,
//                 110,111,113*,121*,122*, (120/3,120/9)
//                 XXX can also include 120/1 (authentication challenge)!
//   130     -/-   2*,4*,11*,13*,22*,23*,32*,33*,42*,43*,51,70/4-7,82,83/1,
//                 85*,88*,111,113*,122*, (120/3,120/9)
//   131     ???   120/1-2,120/5,120/7,120/12,120/15
//                 * excluding variation 0 ("any")
//
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
// as can be seen from the above tables, all function codes except 32
// (AUTHENTICATE_REQ), 33 (AUTH_REQ_NO_ACK), and 313 (AUTHENTICATE_RESP) can
// include the "aggressive mode authentication" objects g120v3 and g120v9. the
// combinator 'ama' defined at the top of this file is used to generically add
// the authentication object parsing.

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
                                     dnp3_p_g12v1_binoutcmd_crob_oblock,
                                     dnp3_p_g12v2_binoutcmd_pcb_oblock,
                                     dnp3_p_g12v3_binoutcmd_pcm_rblock,
                                        // XXX stricter rules for PCB/PCM in responses?
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

    // analog inputs
    H_RULE(rblock_anain,    h_choice(dnp3_p_anain_rblock,
                                     dnp3_p_anainev_rblock,
                                     dnp3_p_frozenanain_rblock,
                                     dnp3_p_frozenanainev_rblock,
                                     dnp3_p_anaindeadband_rblock, NULL));
    H_RULE(oblock_anain,    h_choice(dnp3_p_anain_oblock,
                                     dnp3_p_anainev_oblock,
                                     dnp3_p_frozenanain_oblock,
                                     dnp3_p_frozenanainev_oblock,
                                     dnp3_p_anaindeadband_oblock, NULL));

    // analog outputs
    H_RULE(rblock_anaout,   h_choice(dnp3_p_anaoutstatus_rblock,
                                     dnp3_p_anaoutev_rblock,
                                     dnp3_p_anaoutcmdev_rblock, NULL));
    H_RULE(oblock_anaout,   h_choice(dnp3_p_anaoutstatus_oblock,
                                     dnp3_p_anaout_oblock,
                                     dnp3_p_anaoutev_oblock,
                                     dnp3_p_anaoutcmdev_oblock, NULL));

    // times
    H_RULE(rblock_time,     h_choice(dnp3_p_g50v1_time_rblock,
                                     dnp3_p_g50v4_indexed_time_rblock, NULL));
    H_RULE(oblock_time,     h_choice(dnp3_p_g50v1_time_oblock,
                                     dnp3_p_g50v4_indexed_time_oblock,
                                     dnp3_p_cto_oblock,
                                     dnp3_p_delay_oblock, NULL));
    H_RULE(wblock_time,     h_choice(dnp3_p_g50v1_time_oblock,
                                     dnp3_p_g50v3_recorded_time_oblock,
                                     dnp3_p_g50v4_indexed_time_oblock, NULL));

    // class data
    H_RULE(rblock_class,    h_choice(dnp3_p_g60v1_class0_rblock,
                                     dnp3_p_g60v2_class1_rblock,
                                     dnp3_p_g60v3_class2_rblock,
                                     dnp3_p_g60v4_class3_rblock, NULL));

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
                                             rblock_anain,
                                             rblock_anaout,
                                             rblock_time,
                                             rblock_class,
                                             dnp3_p_iin_rblock,
                                             NULL));
    H_RULE(read,            dnp3_p_many(read_oblock));
    // XXX NB parsing pseudocode in AN2012-004b does NOT work for READ requests.
    //     it misses the case that a function code requires object headers
    //     but no objects. never mind that it might require an object with some
    //     variations but not others (e.g. g70v5 file transmission).

    H_RULE(write_oblock,    dnp3_p_objchoice(//wblock_attr,
                                             dnp3_p_g10v1_binout_packed_oblock,
                                             dnp3_p_anaindeadband_oblock,
                                             wblock_time,   // XXX multiple blocks ok?!
                                             dnp3_p_iin_oblock,
                                             NULL));
    H_RULE(write,           dnp3_p_many(write_oblock));

    #define act_select dnp3_p_act_flatten
    H_RULE(pcb,             dnp3_p_g12v2_binoutcmd_pcb_oblock);
    H_RULE(pcm,             dnp3_p_g12v3_binoutcmd_pcm_oblock);
    H_RULE(select_pcb,      dnp3_p_seq(pcb, dnp3_p_many1(pcm)));
    H_RULE(select_oblock,   dnp3_p_objchoice(select_pcb,
                                             dnp3_p_g12v1_binoutcmd_crob_oblock,
                                             dnp3_p_anaout_oblock,
                                             NULL));
    H_ARULE(select,         dnp3_p_many(select_oblock));
        // XXX empty select requests valid?
        // XXX is it valid to have many pcb-pcm blocks in the same request? to mix pcbs and crobs?

    H_RULE(freezable,       h_choice(dnp3_p_ctr_fblock, dnp3_p_anain_fblock, NULL));
    H_RULE(clearable,       dnp3_p_ctr_fblock);

    H_RULE(freeze,          dnp3_p_many(dnp3_p_objchoice(freezable, NULL)));
    H_RULE(freeze_clear,    dnp3_p_many(dnp3_p_objchoice(clearable, NULL)));

    #define act_freeze_at_time dnp3_p_act_flatten
    H_RULE(tdi,             dnp3_p_g50v2_time_interval_oblock);
    H_RULE(frz_schedule,    dnp3_p_seq(tdi, dnp3_p_many(freezable)));
    H_ARULE(freeze_at_time, dnp3_p_many(frz_schedule));

    H_RULE(cold_restart,    h_epsilon_p());
    H_RULE(warm_restart,    h_epsilon_p());

    H_RULE(rsp_oblock,      dnp3_p_objchoice(//oblock_attr,
                                             oblock_binin,
                                             oblock_binout,
                                             oblock_ctr,
                                             oblock_anain,
                                             oblock_anaout,
                                             oblock_time,
                                             dnp3_p_iin_oblock,
                                             NULL));
    H_RULE(response,        dnp3_p_many(rsp_oblock));

    H_RULE(unsolicited,     h_epsilon_p()); // XXX


    odata[DNP3_CONFIRM] = ama(confirm);
    odata[DNP3_READ]    = ama(read);
    odata[DNP3_WRITE]   = ama(write);
    odata[DNP3_SELECT]            = // -.
    odata[DNP3_OPERATE]           = // -.
    odata[DNP3_DIRECT_OPERATE]    = // -v
    odata[DNP3_DIRECT_OPERATE_NR] = ama(select);
    odata[DNP3_IMMED_FREEZE]      = // -v
    odata[DNP3_IMMED_FREEZE_NR]   = ama(freeze);
    odata[DNP3_FREEZE_CLEAR]      = // -v
    odata[DNP3_FREEZE_CLEAR_NR]   = ama(freeze_clear);
    odata[DNP3_FREEZE_AT_TIME]    = // -v
    odata[DNP3_FREEZE_AT_TIME_NR] = ama(freeze_at_time);
    odata[DNP3_COLD_RESTART]      = ama(cold_restart);
    odata[DNP3_WARM_RESTART]      = ama(warm_restart);
    odata[DNP3_INITIALIZE_DATA]   = NULL;   // obsolete, not supported

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

    return h_make_err(p->arena, ERR_FUNC_NOT_SUPP, frag);
}

// parse the rest of a fragment, after the application header
static HParser *f_fragment(const HParsedToken *hdr, void *env)
{
    // propagate TT_ERR on function code
    HParsedToken *fc_ = H_INDEX_TOKEN(hdr, 1);
    if(H_ISERR(fc_->token_type))
        goto err;

    int fc = H_CAST_UINT(fc_);

    // basic object data parser
    HParser *p = odata[fc];
    if(p == NULL)
        goto err;

    // odata must always parse the entire rest of the fragment
    p = dnp3_p_packet(p);

    // any unspecific parse failure on odata should yield PARAM_ERROR
    p = h_choice(p, h_error(ERR_PARAM_ERROR), NULL);

    return h_action(p, act_fragment, (void *)hdr);

    err: {
        HParsedToken *ac = H_INDEX_TOKEN(hdr, 0);
        return h_action(h_unit(fc_), act_fragment_errfc, (void *)ac);
    }
}

static HParsedToken *act_iin(const HParseResult *p, void *user)
{
    DNP3_IntIndications *iin = H_ALLOC(DNP3_IntIndications);

    iin->broadcast          = H_FIELD_UINT(DNP3_IIN_BROADCAST);
    iin->class1             = H_FIELD_UINT(DNP3_IIN_CLASS1);
    iin->class2             = H_FIELD_UINT(DNP3_IIN_CLASS2);
    iin->class3             = H_FIELD_UINT(DNP3_IIN_CLASS3);
    iin->need_time          = H_FIELD_UINT(DNP3_IIN_NEED_TIME);
    iin->local_ctrl         = H_FIELD_UINT(DNP3_IIN_LOCAL_CTRL);
    iin->device_trouble     = H_FIELD_UINT(DNP3_IIN_DEVICE_TROUBLE);
    iin->device_restart     = H_FIELD_UINT(DNP3_IIN_DEVICE_RESTART);
    iin->func_not_supp      = H_FIELD_UINT(DNP3_IIN_FUNC_NOT_SUPP);
    iin->obj_unknown        = H_FIELD_UINT(DNP3_IIN_OBJ_UNKNOWN);
    iin->param_error        = H_FIELD_UINT(DNP3_IIN_PARAM_ERROR);
    iin->eventbuf_overflow  = H_FIELD_UINT(DNP3_IIN_EVENTBUF_OVERFLOW);
    iin->already_executing  = H_FIELD_UINT(DNP3_IIN_ALREADY_EXECUTING);
    iin->config_corrupt     = H_FIELD_UINT(DNP3_IIN_CONFIG_CORRUPT);
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
    dnp3_p_init_binoutcmd();
    dnp3_p_init_counter();
    dnp3_p_init_analog();
    dnp3_p_init_time();
    dnp3_p_init_class();
    dnp3_p_init_iin();

    // initialize request-specific "object data" parsers
    init_odata();

    H_RULE (bit,    h_bits(1, false));
    H_RULE (zro,    dnp3_p_int_exact(bit, 0));
    H_RULE (one,    dnp3_p_int_exact(bit, 1));
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
    H_RULE (fc_rsp, dnp3_p_int_exact(fc, DNP3_RESPONSE));
    H_RULE (fc_ur,  dnp3_p_int_exact(fc, DNP3_UNSOLICITED_RESPONSE));
    H_RULE (fc_ar,  dnp3_p_int_exact(fc, DNP3_AUTHENTICATE_RESP));

    H_RULE (confc,  dnp3_p_int_exact(fc, DNP3_CONFIRM));
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
