#include <dnp3.h>
#include <stdlib.h>     // malloc
#include <string.h>     // strlen
#include <inttypes.h>   // PRIu32 etc.
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
    case GV(ANAIN, 32BIT_FLAG):
    case GV(ANAIN, 16BIT_FLAG):
    case GV(ANAINEV, 32BIT_FLAG):
    case GV(ANAINEV, 16BIT_FLAG):
    case GV(FROZENANAIN, 32BIT_FLAG):
    case GV(FROZENANAIN, 16BIT_FLAG):
    case GV(FROZENANAINEV, 32BIT_FLAG):
    case GV(FROZENANAINEV, 16BIT_FLAG):
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
    case GV(ANAIN, FLOAT_FLAG):
    case GV(ANAIN, DOUBLE_FLAG):
    case GV(ANAINEV, FLOAT_FLAG):
    case GV(ANAINEV, DOUBLE_FLAG):
    case GV(FROZENANAIN, FLOAT_FLAG):
    case GV(FROZENANAIN, DOUBLE_FLAG):
    case GV(FROZENANAINEV, FLOAT_FLAG):
    case GV(FROZENANAINEV, DOUBLE_FLAG):
        append_flags(&res, &size, o.ana.flags);
        // fall through to append value
    case GV(ANAINDEADBAND, FLOAT):
        appendf(&res, &size, "%.1f", o.ana.flt);
        break;
    case GV(ANAINEV, 32BIT_FLAG_TIME):
    case GV(ANAINEV, 16BIT_FLAG_TIME):
    case GV(FROZENANAIN, 32BIT_FLAG_TIME):
    case GV(FROZENANAIN, 16BIT_FLAG_TIME):
    case GV(FROZENANAINEV, 32BIT_FLAG_TIME):
    case GV(FROZENANAINEV, 16BIT_FLAG_TIME):
        append_flags(&res, &size, o.timed.ana.flags);
        appendf(&res, &size, "%"PRIi32, o.timed.ana.sint);
        append_abstime(&res, &size, o.timed.abstime);
        break;
    case GV(ANAINEV, FLOAT_FLAG_TIME):
    case GV(ANAINEV, DOUBLE_FLAG_TIME):
    case GV(FROZENANAINEV, FLOAT_FLAG_TIME):
    case GV(FROZENANAINEV, DOUBLE_FLAG_TIME):
        append_flags(&res, &size, o.ana.flags);
        appendf(&res, &size, "%.1f", o.ana.flt);
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
