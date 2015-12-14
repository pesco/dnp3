#include <dnp3hammer.h>

#include <stdlib.h>     // malloc
#include <string.h>     // strlen
#include <inttypes.h>   // PRIu32 etc.
#include <ctype.h>
#include <assert.h>
#include "app.h"        // GV()


// human-readable output formatting

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

static int appendf(char **s, size_t *size, const char *fmt, ...)
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
    if(flags.over_range)        appendf(&res, &n, ",over_range");
    if(flags.reference_err)     appendf(&res, &n, ",reference_err");

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

static int append_time(char **res, size_t *size, const char *pre, uint64_t time, bool relative)
{
    uint64_t s  = time / 1000;
    uint64_t ms = time % 1000;
    const char *fmt;

    if(relative)
        fmt = ms ? "%s%"PRIu64".%.3"PRIu64"s" : "%s%"PRIu64"s";
    else
        fmt = ms ? "%s%"PRIu64".%.3"PRIu64"s" : "%s%"PRIu64"s";
    return appendf(res, size, fmt, pre, s, ms);
}

#define append_abstime(res, size, time) append_time(res, size, "@", time, false)
#define append_reltime(res, size, time) append_time(res, size, "@+", time, true)
#define append_interval_ms(res, size, time) append_time(res, size, "+", time, true)

static int append_interval(char **res, size_t *size, uint32_t val, DNP3_IntervalUnit unit)
{
    const char *u = NULL;

    switch(unit) {
    case DNP3_INTERVAL_NONE:                return 0;
    case DNP3_INTERVAL_MILLISECONDS:        u = "ms"; break;
    case DNP3_INTERVAL_SECONDS:             u = "s"; break;
    case DNP3_INTERVAL_MINUTES:             u = "min"; break;
    case DNP3_INTERVAL_HOURS:               u = "h"; break;
    case DNP3_INTERVAL_DAYS:                u = "d"; break;
    case DNP3_INTERVAL_WEEKS:               u = "w"; break;
    case DNP3_INTERVAL_MONTHS:              u = "months"; break;
    case DNP3_INTERVAL_MONTHS_START_DOW:    u = "months(same-dow-from-start)"; break;
    case DNP3_INTERVAL_MONTHS_END_DOW:      u = "months(same-dow-from-end)"; break;
    case DNP3_INTERVAL_SEASONS:             u = "seasons"; break;
    default:                                u = "[?]";
    }

    return appendf(res, size, "+%"PRIu32"%s", val, u);
}

static int append_crob(char **res, size_t *size, DNP3_Command crob)
{
    static const char *tcc_s[] = {"", "CLOSE ", "TRIP ", "XXX "};
    static const char *optype_s[] = {
        "", "PULSE_ON ", "PULSE_OFF ", "LATCH_ON ", "LATCH_OFF ",
        "OP 5 ", "OP 6 ", "OP 7 ", "OP 8 ", "OP 9 ", "OP 10 ",
        "OP 11 ", "OP 12 ", "OP 13 ", "OP 14 ", "OP 15 " };

    const char *tcc = tcc_s[crob.tcc];
    const char *optype = optype_s[crob.optype];
    const char *queue = crob.queue? "queue " : "";
    const char *clear = crob.clear? "clear " : "";

    char status[16] = {0};
    if(crob.status)
        snprintf(status, sizeof(status), " status=%d", crob.status);

    return appendf(res, size, "(%s%s%s%s%dx on=%dms off=%dms%s)", tcc, optype,
                   queue, clear, crob.count, crob.on, crob.off, status);
}

static int append_string(char **res, size_t *size, const char *s, size_t n)
{
    char *t = malloc(n+1);
    if(!t) return -1;

    for(size_t i=0; i<n; i++) {
        t[i] = isalnum(s[i]) ? s[i] : '.'; // XXX escape properly
    }
    t[n] = '\0';

    appendf(res, size, "'%s'", t);
    free(t);
}

char *dnp3_format_object(DNP3_Group g, DNP3_Variation v, const DNP3_Object o)
{
    size_t size;
    char *res = NULL;
    size_t fsize;

    switch(g << 8 | v) {
    case GV(BININ, PACKED):
    case GV(BINOUT, PACKED):
    case GV(BINOUTCMD, PCM):
    case GV(IIN, PACKED):
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
    case GV(BINOUTCMD, CROB):
    case GV(BINOUTCMD, PCB):
        append_crob(&res, &size, o.cmd);
        break;
    case GV(BINOUTCMDEV, NOTIME):
        if(o.cmdev.status)
            appendf(&res, &size, "(status=%d)", (int)o.cmdev.status);
        appendf(&res, &size, "%d", (int)o.cmdev.cs);
        break;
    case GV(BINOUTCMDEV, ABSTIME):
        if(o.timed.cmdev.status)
            appendf(&res, &size, "(status=%d)", (int)o.timed.cmdev.status);
        appendf(&res, &size, "%d", (int)o.timed.cmdev.cs);
        append_abstime(&res, &size, o.timed.abstime);
        break;
    case GV(CTR, 32BIT):
    case GV(CTR, 16BIT):
    case GV(CTREV, 16BIT):
    case GV(CTREV, 32BIT):
    case GV(FROZENCTR, 32BIT):
    case GV(FROZENCTR, 16BIT):
    case GV(FROZENCTREV, 32BIT):
    case GV(FROZENCTREV, 16BIT):
        append_flags(&res, &size, o.ctr.flags);
        // fall through to next case to append counter value
    case GV(CTR, 32BIT_NOFLAG):
    case GV(CTR, 16BIT_NOFLAG):
    case GV(FROZENCTR, 32BIT_NOFLAG):
    case GV(FROZENCTR, 16BIT_NOFLAG):
        appendf(&res, &size, "%"PRIu64, o.ctr.value);
        break;
    case GV(CTREV, 16BIT_TIME):
    case GV(CTREV, 32BIT_TIME):
    case GV(FROZENCTR, 32BIT_TIME):
    case GV(FROZENCTR, 16BIT_TIME):
    case GV(FROZENCTREV, 32BIT_TIME):
    case GV(FROZENCTREV, 16BIT_TIME):
        append_flags(&res, &size, o.timed.ctr.flags);
        appendf(&res, &size, "%"PRIu64, o.timed.ctr.value);
        append_abstime(&res, &size, o.timed.abstime);
        break;
    case GV(ANAIN, 32BIT):
    case GV(ANAIN, 16BIT):
    case GV(ANAINEV, 32BIT):
    case GV(ANAINEV, 16BIT):
    case GV(FROZENANAIN, 32BIT):
    case GV(FROZENANAIN, 16BIT):
    case GV(FROZENANAINEV, 32BIT):
    case GV(FROZENANAINEV, 16BIT):
    case GV(ANAOUTSTATUS, 32BIT):
    case GV(ANAOUTSTATUS, 16BIT):
    case GV(ANAOUTEV, 32BIT):
    case GV(ANAOUTEV, 16BIT):
        append_flags(&res, &size, o.ana.flags);
        // fall through to next case to append value
    case GV(ANAIN, 32BIT_NOFLAG):
    case GV(ANAIN, 16BIT_NOFLAG):
    case GV(FROZENANAIN, 32BIT_NOFLAG):
    case GV(FROZENANAIN, 16BIT_NOFLAG):
    case GV(ANAINDEADBAND, 32BIT):
    case GV(ANAINDEADBAND, 16BIT):
        appendf(&res, &size, "%"PRIi32, o.ana.sint);
        break;
    case GV(ANAIN, FLOAT):
    case GV(ANAIN, DOUBLE):
    case GV(ANAINEV, FLOAT):
    case GV(ANAINEV, DOUBLE):
    case GV(FROZENANAIN, FLOAT):
    case GV(FROZENANAIN, DOUBLE):
    case GV(FROZENANAINEV, FLOAT):
    case GV(FROZENANAINEV, DOUBLE):
    case GV(ANAOUTSTATUS, FLOAT):
    case GV(ANAOUTSTATUS, DOUBLE):
    case GV(ANAOUTEV, FLOAT):
    case GV(ANAOUTEV, DOUBLE):
        append_flags(&res, &size, o.ana.flags);
        // fall through to append value
    case GV(ANAINDEADBAND, FLOAT):
        appendf(&res, &size, "%.1f", o.ana.flt);
        break;
    case GV(ANAINEV, 32BIT_TIME):
    case GV(ANAINEV, 16BIT_TIME):
    case GV(FROZENANAIN, 32BIT_TIME):
    case GV(FROZENANAIN, 16BIT_TIME):
    case GV(FROZENANAINEV, 32BIT_TIME):
    case GV(FROZENANAINEV, 16BIT_TIME):
    case GV(ANAOUTEV, 32BIT_TIME):
    case GV(ANAOUTEV, 16BIT_TIME):
        append_flags(&res, &size, o.timed.ana.flags);
        appendf(&res, &size, "%"PRIi32, o.timed.ana.sint);
        append_abstime(&res, &size, o.timed.abstime);
        break;
    case GV(ANAINEV, FLOAT_TIME):
    case GV(ANAINEV, DOUBLE_TIME):
    case GV(FROZENANAINEV, FLOAT_TIME):
    case GV(FROZENANAINEV, DOUBLE_TIME):
    case GV(ANAOUTEV, FLOAT_TIME):
    case GV(ANAOUTEV, DOUBLE_TIME):
        append_flags(&res, &size, o.ana.flags);
        appendf(&res, &size, "%.1f", o.ana.flt);
        append_abstime(&res, &size, o.timed.abstime);
        break;
    case GV(ANAOUTCMDEV, 32BIT):
    case GV(ANAOUTCMDEV, 16BIT):
    case GV(ANAOUT, 32BIT):
    case GV(ANAOUT, 16BIT):
        if(o.ana.status)
            appendf(&res, &size, "(status=%d)", (int)o.ana.status);
        appendf(&res, &size, "%"PRIi32, o.ana.sint);
        break;
    case GV(ANAOUTCMDEV, 32BIT_TIME):
    case GV(ANAOUTCMDEV, 16BIT_TIME):
        if(o.ana.status)
            appendf(&res, &size, "(status=%d)", (int)o.timed.ana.status);
        appendf(&res, &size, "%"PRIi32, o.timed.ana.sint);
        append_abstime(&res, &size, o.timed.abstime);
        break;
    case GV(ANAOUTCMDEV, FLOAT):
    case GV(ANAOUTCMDEV, DOUBLE):
    case GV(ANAOUT, FLOAT):
    case GV(ANAOUT, DOUBLE):
        if(o.ana.status)
            appendf(&res, &size, "(status=%d)", (int)o.ana.status);
        appendf(&res, &size, "%.1f", o.ana.flt);
        break;
    case GV(ANAOUTCMDEV, FLOAT_TIME):
    case GV(ANAOUTCMDEV, DOUBLE_TIME):
        if(o.timed.ana.status)
            appendf(&res, &size, "(status=%d)", (int)o.timed.ana.status);
        appendf(&res, &size, "%.1f", o.timed.ana.flt);
        append_abstime(&res, &size, o.timed.abstime);
        break;
    case GV(TIME, TIME):
    case GV(TIME, RECORDED_TIME):
        append_abstime(&res, &size, o.time.abstime);
        break;
    case GV(TIME, TIME_INTERVAL):
        append_abstime(&res, &size, o.time.abstime);
        append_interval_ms(&res, &size, o.time.interval);
        break;
    case GV(TIME, INDEXED_TIME):
        append_abstime(&res, &size, o.time.abstime);
        append_interval(&res, &size, o.time.interval, o.time.unit);
        break;
    case GV(CTO, UNSYNC):
        appendf(&res, &size, "(unsynchronized)");
        // fall through and append time
    case GV(CTO, SYNC):
        append_abstime(&res, &size, o.time.abstime);
        break;
    case GV(DELAY, S):
    case GV(DELAY, MS):
        appendf(&res, &size, "%"PRIu32"ms", o.delay);
        break;
    case GV(APPL, ID):
        append_string(&res, &size, o.applid.str, o.applid.len);
        break;
    }

    if(!res)
        appendf(&res, &size, "?");

    return res;
}

char *dnp3_format_oblock_(const DNP3_ObjectBlock *ob, bool do_data)
{
    size_t size;
    char *res = NULL;
    const char *sep = ob->objects ? ":" : "";
    int x;

    // group, variation, qc
    x = appendf(&res, &size, "g%dv%d qc=%X%X",
                (int)ob->group, (int)ob->variation,
                (unsigned int)ob->prefixcode, (unsigned int)ob->rangespec);
    if(x<0) goto err;

    if(!do_data) return res;

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
    } else if(ob->prefixcode == 0 && ob->rangespec >= 7 && ob->rangespec <= 9) {
        // count field but no objects or indexes
        // (presumably this is on a request giving a maximum number of objects.)
        x = appendf(&res, &size, " range=%d", ob->count);
        if(x<0) goto err;
    }

    return res;

err:
    if(res) free(res);
    return NULL;
}

char *dnp3_format_oblock(const DNP3_ObjectBlock *ob)
{ return dnp3_format_oblock_(ob, true); }

char *dnp3_format_fragment_(const DNP3_Fragment *frag, bool do_objects, bool do_data)
{
    char *res = NULL;
    size_t size;
    int x;

    // flags string
    char flags[20]; // need 4*3(names)+3(seps)+2(parens)+1(space)+1(null)
    char *p = flags;
    if(frag->ac.fir) { strcpy(p, ",fir"); p+=4; }
    if(frag->ac.fin) { strcpy(p, ",fin"); p+=4; }
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
    if(do_objects) {
        for(size_t i=0; i<frag->nblocks; i++) {
            char *blk = dnp3_format_oblock_(frag->odata[i], do_data);
            if(!blk) goto err;

            x = appendf(&res, &size, " {%s}", blk);
            free(blk);
            if(x<0) goto err;
        }
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

char *dnp3_format_fragment(const DNP3_Fragment *frag)
{ return dnp3_format_fragment_(frag, true, true); }

char *dnp3_format_fragment_ohdrs(const DNP3_Fragment *frag)
{ return dnp3_format_fragment_(frag, false, true); }

char *dnp3_format_fragment_header(const DNP3_Fragment *frag)
{ return dnp3_format_fragment_(frag, false, false); }

int append_payload(char **res, size_t *size, uint8_t *bytes, size_t len)
{
    if(bytes) {
        char *s = malloc(len * 3 + 1);
        if(!s) return -1;

        char *p = s;
        *p = '\0';
        for(size_t i=0; i<len; i++)
            p += sprintf(p, " %.2X", bytes[i]);

        int x = appendf(res, size, ":%s", s);
        free(s);
        return x;
    } else {
        int x = appendf(res, size, ": (null)");
        return x;
    }
}

char *dnp3_format_segment_(const DNP3_Segment *seg, bool do_payload)
{
    char *res = NULL;
    size_t size;
    int x;

    const char *flags = "";
    if(seg->fir && seg->fin) flags = "(fir,fin) ";
    else if(seg->fir)        flags = "(fir) ";
    else if(seg->fin)        flags = "(fin) ";

    x = appendf(&res, &size, "%ssegment %"PRIu8, flags, seg->seq);
    if(x<0) goto err;

    if(do_payload) {
        x = append_payload(&res, &size, seg->payload, seg->len);
        if(x<0) goto err;
    }

    return res;

err:
    if(res) free(res);
    return NULL;
}

char *dnp3_format_segment(const DNP3_Segment *seg)
{ return dnp3_format_segment_(seg, true); }

char *dnp3_format_segment_header(const DNP3_Segment *seg)
{ return dnp3_format_segment_(seg, false); }

static const char *linkfuncnames[32] = {
    // secondary (PRM=0)
    "ACK", "NACK", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    "LINK_STATUS", NULL, NULL, "function 14 (obsolete)", "NOT_SUPPORTED",

    // primary (PRM=1)
    "RESET_LINK_STATES", "function 1 (obsolete)", "TEST_LINK_STATES",
    "CONFIRMED_USER_DATA", "UNCONFIRMED_USER_DATA", NULL, NULL, NULL, NULL,
    "REQUEST_LINK_STATUS", NULL, NULL, NULL, NULL, NULL, NULL
};

char *dnp3_format_frame_(const DNP3_Frame *frame, bool do_payload)
{
    char *res = NULL;
    size_t size;
    int x;

    // header
    x = appendf(&res, &size, "%s frame from %s %"PRIu16" to %"PRIu16": ",
                             dnp3_link_prm(frame)? "primary" : "secondary",
                             frame->dir? "master" : "outstation",
                             frame->source, frame->destination);
    if(x<0) goto err;

    // frame count / data flow control flags
    if(dnp3_link_prm(frame)) {
        if(frame->fcv) {
            x = appendf(&res, &size, "(fcb=%d) ", (int)frame->fcb);
            if(x<0) goto err;
        }
    } else {
        if(frame->dfc && frame->fcb)
            x = appendf(&res, &size, "(fcb=1,dfc) ");
        else if(frame->fcb)
            x = appendf(&res, &size, "(fcb=1) ");
        else if(frame->dfc)
            x = appendf(&res, &size, "(dfc) ");
        if(x<0) goto err;
    }

    // function name
    const char *name = linkfuncnames[frame->func];
    if(name)
        x = appendf(&res, &size, "%s", name);
    else
        x = appendf(&res, &size, "function %d (reserved)", (int)(frame->func & 0xF));
    if(x<0) goto err;

    // user data
    if(do_payload && frame->len > 0) {
        if(frame->payload)
            x = append_payload(&res, &size, frame->payload, frame->len);
        else
            x = appendf(&res, &size, ": <corrupt>");
        if(x<0) goto err;
    }

    return res;

err:
    if(res) free(res);
    return NULL;
}

char *dnp3_format_frame(const DNP3_Frame *frame)
{ return dnp3_format_frame_(frame, true); }

char *dnp3_format_frame_header(const DNP3_Frame *frame)
{ return dnp3_format_frame_(frame, false); }
