#ifndef DNP3_H_SEEN
#define DNP3_H_SEEN

#include <hammer/hammer.h>


/// TYPES ///

// data link layer...

typedef enum {
    // used in primary frames:
    DNP3_RESET_LINK_STATES = 0,
    // 1 obsolete
    DNP3_TEST_LINK_STATES = 2,
    DNP3_CONFIRMED_USER_DATA = 3,
    DNP3_UNCONFIRMED_USER_DATA = 4,
    DNP3_REQUEST_LINK_STATUS = 9,

    // used in secondary frames:
    DNP3_ACK = 0,
    DNP3_NACK = 1,
    DNP3_LINK_STATUS = 11,
    DNP3_NOT_SUPPORTED = 15
} DNP3_LinkLayerFunctionCode;

typedef struct {
    uint8_t dir:1;      // direction: 1 = master to outstation
    uint8_t prm:1;      // primary message bit (initiating a transaction?)
    uint8_t fcb:1;      // frame count bit
    union {
        uint8_t fcv:1;  // primary frames: frame count valid?
        uint8_t dfc:1;  // secondary frames: data flow control: 1 = busy
    };
    uint8_t fc:4;       // link layer function code
    uint16_t source;
    uint16_t destination;

    uint8_t len;        // no. of bytes in payload (NOT eq. to the header field)
    uint8_t *payload;
} DNP3_Frame;

// transport function...

typedef struct {
    uint8_t fir:1;  // first segment in series?
    uint8_t fin:1;  // final segment in series?
    uint8_t seq:6;  // 0-63

    size_t len;     // no. of bytes in payload
    uint8_t *payload;
} DNP3_Segment;

// application layer...

typedef struct {
    uint8_t fir:1;  // note: ignore on unsolicited responses (treat as 1)!
    uint8_t fin:1;  // note: ignore on unsolicited responses (treat as 1)!
    uint8_t con:1;
    uint8_t uns:1;
    uint8_t seq:4;
} DNP3_AppControl;

typedef struct {
    // first octet
    uint16_t broadcast:1;
    uint16_t class1:1;
    uint16_t class2:1;
    uint16_t class3:1;
    uint16_t need_time:1;
    uint16_t local_ctrl:1;
    uint16_t device_trouble:1;
    uint16_t device_restart:1;

    // second octet
    uint16_t func_not_supp:1;       // |
    uint16_t obj_unknown:1;         // |- potential parse errors
    uint16_t param_error:1;         // |
    uint16_t eventbuf_overflow:1;
    uint16_t already_executing:1;
    uint16_t config_corrupt:1;
} DNP3_IntIndications;

// bit indexes of internal indication flags, also used in g80v1
typedef enum {
    DNP3_IIN_BROADCAST = 0,
    DNP3_IIN_CLASS1 = 1,
    DNP3_IIN_CLASS2 = 2,
    DNP3_IIN_CLASS3 = 3,
    DNP3_IIN_NEED_TIME = 4,
    DNP3_IIN_LOCAL_CTRL = 5,
    DNP3_IIN_DEVICE_TROUBLE = 6,
    DNP3_IIN_DEVICE_RESTART = 7,
    DNP3_IIN_FUNC_NOT_SUPP = 8,
    DNP3_IIN_OBJ_UNKNOWN = 9,
    DNP3_IIN_PARAM_ERROR = 10,
    DNP3_IIN_EVENTBUF_OVERFLOW = 11,
    DNP3_IIN_ALREADY_EXECUTING = 12,
    DNP3_IIN_CONFIG_CORRUPT = 13
} DNP3_IntIndicationIndex;

typedef enum {
    ERR_FUNC_NOT_SUPP = TT_ERR+1,
    ERR_OBJ_UNKNOWN,
    ERR_PARAM_ERROR
} DNP3_ParseError;

typedef enum {
    // request function codes: 0x00-0x21
    DNP3_CONFIRM    = 0x00,     // confirmation is nominally a request because
                                // it's a master-to-outstation message
    DNP3_READ       = 0x01,
    DNP3_WRITE      = 0x02,
    DNP3_SELECT     = 0x03,
    DNP3_OPERATE    = 0x04,
    DNP3_DIRECT_OPERATE     = 0x05,     // needs no SELECT
    DNP3_DIRECT_OPERATE_NR  = 0x06,     // needs no SELECT and no response
    DNP3_IMMED_FREEZE       = 0x07,
    DNP3_IMMED_FREEZE_NR    = 0x08,     // needs no response
    DNP3_FREEZE_CLEAR       = 0x09,
    DNP3_FREEZE_CLEAR_NR    = 0x0A,     // needs no response
    DNP3_FREEZE_AT_TIME     = 0x0B,
    DNP3_FREEZE_AT_TIME_NR  = 0x0C,     // needs no response
    DNP3_COLD_RESTART       = 0x0D,
    DNP3_WARM_RESTART       = 0x0E,
    DNP3_INITIALIZE_DATA    = 0x0F,
    DNP3_INITIALIZE_APPL    = 0x10,
    DNP3_START_APPL         = 0x11,
    DNP3_STOP_APPL          = 0x12,
    DNP3_SAVE_CONFIG        = 0x13,
    DNP3_ENABLE_UNSOLICITED     = 0x14,
    DNP3_DISABLE_UNSOLICITED    = 0x15,
    DNP3_ASSIGN_CLASS           = 0x16,
    DNP3_DELAY_MEASURE          = 0x17,
    DNP3_RECORD_CURRENT_TIME    = 0x18,
    DNP3_OPEN_FILE              = 0x19,
    DNP3_CLOSE_FILE             = 0x1A,
    DNP3_DELETE_FILE            = 0x1B,
    DNP3_GET_FILE_INFO          = 0x1C,
    DNP3_AUTHENTICATE_FILE      = 0x1D,
    DNP3_ABORT_FILE             = 0x1E,
    DNP3_ACTIVATE_CONFIG        = 0x1F,
    DNP3_AUTHENTICATE_REQ       = 0x20,
    DNP3_AUTH_REQ_NO_ACK        = 0x21,

    // response function codes: 0x81-0x83
    DNP3_RESPONSE               = 0x81,
    DNP3_UNSOLICITED_RESPONSE   = 0x82,
    DNP3_AUTHENTICATE_RESP      = 0x83
} DNP3_FunctionCode;

// group numbers
typedef enum {
    DNP3_GROUP_ATTR = 0,
    DNP3_GROUP_BININ = 1,
    DNP3_GROUP_BININEV = 2,
    DNP3_GROUP_DBLBITIN = 3,
    DNP3_GROUP_DBLBITINEV = 4,
    DNP3_GROUP_BINOUT = 10,
    DNP3_GROUP_BINOUTEV = 11,
    DNP3_GROUP_BINOUTCMD = 12,
    DNP3_GROUP_BINOUTCMDEV = 13,
    DNP3_GROUP_CTR = 20,
    DNP3_GROUP_FROZENCTR = 21,
    DNP3_GROUP_CTREV = 22,
    DNP3_GROUP_FROZENCTREV = 23,
    DNP3_GROUP_ANAIN = 30,
    DNP3_GROUP_FROZENANAIN = 31,
    DNP3_GROUP_ANAINEV = 32,
    DNP3_GROUP_FROZENANAINEV = 33,
    DNP3_GROUP_ANAINDEADBAND = 34,
    DNP3_GROUP_ANAOUTSTATUS = 40,
    DNP3_GROUP_ANAOUT = 41,
    DNP3_GROUP_ANAOUTEV = 42,
    DNP3_GROUP_ANAOUTCMDEV = 43,
    DNP3_GROUP_TIME = 50,
    DNP3_GROUP_CTO = 51,
    DNP3_GROUP_DELAY = 52,
    DNP3_GROUP_CLASS = 60,
    DNP3_GROUP_FILE = 70,
    DNP3_GROUP_IIN = 80,
    DNP3_GROUP_APPL = 90,
    DNP3_GROUP_AUTH = 120,
} DNP3_Group;

// variation numbers
typedef enum {
    DNP3_VARIATION_ANY = 0,

    DNP3_VARIATION_ATTR_ALL = 254,

    DNP3_VARIATION_BININ_PACKED = 1,
    DNP3_VARIATION_BININ_FLAGS = 2,

    DNP3_VARIATION_BININEV_NOTIME = 1,
    DNP3_VARIATION_BININEV_ABSTIME = 2,
    DNP3_VARIATION_BININEV_RELTIME = 3,

    DNP3_VARIATION_DBLBITIN_PACKED = 1,
    DNP3_VARIATION_DBLBITIN_FLAGS = 2,

    DNP3_VARIATION_DBLBITINEV_NOTIME = 1,
    DNP3_VARIATION_DBLBITINEV_ABSTIME = 2,
    DNP3_VARIATION_DBLBITINEV_RELTIME = 3,

    DNP3_VARIATION_BINOUT_PACKED = 1,
    DNP3_VARIATION_BINOUT_FLAGS = 2,

    DNP3_VARIATION_BINOUTEV_NOTIME = 1,
    DNP3_VARIATION_BINOUTEV_ABSTIME = 2,

    DNP3_VARIATION_BINOUTCMD_CROB = 1,
    DNP3_VARIATION_BINOUTCMD_PCB = 2,
    DNP3_VARIATION_BINOUTCMD_PCM = 3,

    DNP3_VARIATION_BINOUTCMDEV_NOTIME = 1,
    DNP3_VARIATION_BINOUTCMDEV_ABSTIME = 2,

    DNP3_VARIATION_CTR_32BIT = 1,
    DNP3_VARIATION_CTR_16BIT = 2,
    DNP3_VARIATION_CTR_32BIT_NOFLAG = 5,
    DNP3_VARIATION_CTR_16BIT_NOFLAG = 6,

    DNP3_VARIATION_FROZENCTR_32BIT = 1,
    DNP3_VARIATION_FROZENCTR_16BIT = 2,
    DNP3_VARIATION_FROZENCTR_32BIT_TIME = 5,
    DNP3_VARIATION_FROZENCTR_16BIT_TIME = 6,
    DNP3_VARIATION_FROZENCTR_32BIT_NOFLAG = 9,
    DNP3_VARIATION_FROZENCTR_16BIT_NOFLAG = 10,

    DNP3_VARIATION_CTREV_32BIT = 1,
    DNP3_VARIATION_CTREV_16BIT = 2,
    DNP3_VARIATION_CTREV_32BIT_TIME = 5,
    DNP3_VARIATION_CTREV_16BIT_TIME = 6,

    DNP3_VARIATION_FROZENCTREV_32BIT = 1,
    DNP3_VARIATION_FROZENCTREV_16BIT = 2,
    DNP3_VARIATION_FROZENCTREV_32BIT_TIME = 5,
    DNP3_VARIATION_FROZENCTREV_16BIT_TIME = 6,

    DNP3_VARIATION_ANAIN_32BIT = 1,
    DNP3_VARIATION_ANAIN_16BIT = 2,
    DNP3_VARIATION_ANAIN_32BIT_NOFLAG = 3,
    DNP3_VARIATION_ANAIN_16BIT_NOFLAG = 4,
    DNP3_VARIATION_ANAIN_FLOAT = 5,
    DNP3_VARIATION_ANAIN_DOUBLE = 6,

    DNP3_VARIATION_FROZENANAIN_32BIT = 1,
    DNP3_VARIATION_FROZENANAIN_16BIT = 2,
    DNP3_VARIATION_FROZENANAIN_32BIT_TIME = 3,
    DNP3_VARIATION_FROZENANAIN_16BIT_TIME = 4,
    DNP3_VARIATION_FROZENANAIN_32BIT_NOFLAG = 5,
    DNP3_VARIATION_FROZENANAIN_16BIT_NOFLAG = 6,
    DNP3_VARIATION_FROZENANAIN_FLOAT = 7,
    DNP3_VARIATION_FROZENANAIN_DOUBLE = 8,

    DNP3_VARIATION_ANAINEV_32BIT = 1,
    DNP3_VARIATION_ANAINEV_16BIT = 2,
    DNP3_VARIATION_ANAINEV_32BIT_TIME = 3,
    DNP3_VARIATION_ANAINEV_16BIT_TIME = 4,
    DNP3_VARIATION_ANAINEV_FLOAT = 5,
    DNP3_VARIATION_ANAINEV_DOUBLE = 6,
    DNP3_VARIATION_ANAINEV_FLOAT_TIME = 7,
    DNP3_VARIATION_ANAINEV_DOUBLE_TIME = 8,

    DNP3_VARIATION_FROZENANAINEV_32BIT = 1,
    DNP3_VARIATION_FROZENANAINEV_16BIT = 2,
    DNP3_VARIATION_FROZENANAINEV_32BIT_TIME = 3,
    DNP3_VARIATION_FROZENANAINEV_16BIT_TIME = 4,
    DNP3_VARIATION_FROZENANAINEV_FLOAT = 5,
    DNP3_VARIATION_FROZENANAINEV_DOUBLE = 6,
    DNP3_VARIATION_FROZENANAINEV_FLOAT_TIME = 7,
    DNP3_VARIATION_FROZENANAINEV_DOUBLE_TIME = 8,

    DNP3_VARIATION_ANAINDEADBAND_16BIT = 1,
    DNP3_VARIATION_ANAINDEADBAND_32BIT = 2,
    DNP3_VARIATION_ANAINDEADBAND_FLOAT = 3,

    DNP3_VARIATION_ANAOUTSTATUS_32BIT = 1,
    DNP3_VARIATION_ANAOUTSTATUS_16BIT = 2,
    DNP3_VARIATION_ANAOUTSTATUS_FLOAT = 3,
    DNP3_VARIATION_ANAOUTSTATUS_DOUBLE = 4,

    DNP3_VARIATION_ANAOUT_32BIT = 1,
    DNP3_VARIATION_ANAOUT_16BIT = 2,
    DNP3_VARIATION_ANAOUT_FLOAT = 3,
    DNP3_VARIATION_ANAOUT_DOUBLE = 4,

    DNP3_VARIATION_ANAOUTEV_32BIT = 1,
    DNP3_VARIATION_ANAOUTEV_16BIT = 2,
    DNP3_VARIATION_ANAOUTEV_32BIT_TIME = 3,
    DNP3_VARIATION_ANAOUTEV_16BIT_TIME = 4,
    DNP3_VARIATION_ANAOUTEV_FLOAT = 5,
    DNP3_VARIATION_ANAOUTEV_DOUBLE = 6,
    DNP3_VARIATION_ANAOUTEV_FLOAT_TIME = 7,
    DNP3_VARIATION_ANAOUTEV_DOUBLE_TIME = 8,

    DNP3_VARIATION_ANAOUTCMDEV_32BIT = 1,
    DNP3_VARIATION_ANAOUTCMDEV_16BIT = 2,
    DNP3_VARIATION_ANAOUTCMDEV_32BIT_TIME = 3,
    DNP3_VARIATION_ANAOUTCMDEV_16BIT_TIME = 4,
    DNP3_VARIATION_ANAOUTCMDEV_FLOAT = 5,
    DNP3_VARIATION_ANAOUTCMDEV_DOUBLE = 6,
    DNP3_VARIATION_ANAOUTCMDEV_FLOAT_TIME = 7,
    DNP3_VARIATION_ANAOUTCMDEV_DOUBLE_TIME = 8,

    DNP3_VARIATION_TIME_TIME = 1,
    DNP3_VARIATION_TIME_TIME_INTERVAL = 2,
    DNP3_VARIATION_TIME_RECORDED_TIME = 3,
    DNP3_VARIATION_TIME_INDEXED_TIME = 4,

    DNP3_VARIATION_CTO_SYNC = 1,
    DNP3_VARIATION_CTO_UNSYNC = 2,

    DNP3_VARIATION_DELAY_S = 1,
    DNP3_VARIATION_DELAY_MS = 2,

    DNP3_VARIATION_CLASS_0 = 1,
    DNP3_VARIATION_CLASS_1 = 2,
    DNP3_VARIATION_CLASS_2 = 3,
    DNP3_VARIATION_CLASS_3 = 4,

    DNP3_VARIATION_IIN_PACKED = 1,

    DNP3_VARIATION_APPL_ID = 1,

    DNP3_VARIATION_AUTH_AGGR = 3,
    DNP3_VARIATION_AUTH_MAC = 9,
} DNP3_Variation;

typedef uint64_t DNP3_Time;     // milliseconds since 1970-01-01

typedef enum {
    DNP3_INTERMEDIATE = 0,
    DNP3_DETERMINED_OFF = 1,
    DNP3_DETERMINED_ON = 2,
    DNP3_INDETERMINATE = 3
} DNP3_DblBit;

typedef enum {
    DNP3_CTL_SUCCESS = 0,
    DNP3_CTL_TIMEOUT = 1,
    DNP3_CTL_NO_SELECT = 2,
    DNP3_CTL_FORMAT_ERROR = 3,
    DNP3_CTL_NOT_SUPPORTED = 4,
    DNP3_CTL_ALREADY_ACTIVE = 5,
    DNP3_CTL_HARDWARE_ERROR = 6,
    DNP3_CTL_LOCAL = 7,
    DNP3_CTL_TOO_MANY_OBJS = 8,
    DNP3_CTL_NOT_AUTHORIZED = 9,
    DNP3_CTL_AUTOMATION_INHIBIT = 10,
    DNP3_CTL_PROCESSING_LIMITED = 11,
    DNP3_CTL_OUT_OF_RANGE = 12,
    DNP3_CTL_NOT_PARTICIPATING = 126
} DNP3_ControlStatus;

typedef enum {
    DNP3_INTERVAL_NONE = 0,
    DNP3_INTERVAL_MILLISECONDS = 1,
    DNP3_INTERVAL_SECONDS = 2,
    DNP3_INTERVAL_MINUTES = 3,
    DNP3_INTERVAL_HOURS = 4,
    DNP3_INTERVAL_DAYS = 5,
    DNP3_INTERVAL_WEEKS = 6,
    DNP3_INTERVAL_MONTHS = 7,
    DNP3_INTERVAL_MONTHS_START_DOW = 8,
    DNP3_INTERVAL_MONTHS_END_DOW = 9,
    DNP3_INTERVAL_SEASONS = 10,
    // 11-127 reserved
    // 128-255 reserved for supplier-specific uses
    DNP3_INTERVAL_SUPPLIER = 128,
    DNP3_INTERVAL_MAX = 255
} DNP3_IntervalUnit;

// flags used with various objects
typedef struct {
    uint8_t online:1;
    uint8_t restart:1;
    uint8_t comm_lost:1;
    uint8_t remote_forced:1;
    uint8_t local_forced:1;

    // for binary inputs
    uint8_t chatter_filter:1;

    // for counters
    uint8_t discontinuity:1;

    // for analog inputs
    uint8_t over_range:1;
    uint8_t reference_err:1;

    // XXX move 'state' out of this struct
    uint8_t state:2;            // single-bit binary (0/1) or DNP3_DblBit!
} DNP3_Flags;

typedef struct {
    uint8_t cs:1;                   // "commanded state"
    DNP3_ControlStatus status:7;
} DNP3_CommandEvent;

typedef struct {
    uint8_t optype:4;
    uint8_t queue:1;
        // the queue flag is obsolete - masters should never set it and
        // outstations should return status NOT_SUPPORTED when it is set.
    uint8_t clear:1;
    uint8_t tcc:2;
    uint8_t count;
    uint32_t on;        // [ms]
    uint32_t off;       // [ms]
    DNP3_ControlStatus status;
} DNP3_Command;

typedef struct {
    DNP3_Flags flags;
    uint32_t value;
} DNP3_Counter;

typedef struct {
    DNP3_Flags flags;
    DNP3_ControlStatus status;
    union {
        int32_t  sint;
        uint32_t uint;  // only used for deadband values (g34v1, g34v2)
        double   flt;   // also used for deadband values (g34v3)
    };
} DNP3_Analog;

typedef union {
    // g1v1, g10v1 (binary in- and outputs, packed format)
    uint8_t bit:1;

    // g3v1 (double-bit binary, packed format)
    DNP3_DblBit dblbit:2;

    // g1v2, g2v1, g3v2, g4v1, g10v2 (binary in- and outputs, with flags)
    DNP3_Flags flags;

    // g12v1, g12v2 (binary output command)
    DNP3_Command cmd;

    // g13v1 (binary output command event)
    DNP3_CommandEvent cmdev;

    // g20v1, g20v2, g20v5, g20v6, g21v1, g21v2, g21v9, g21v10 (counters)
    DNP3_Counter ctr;

    // analog in- and outputs
    DNP3_Analog ana;

    // g50, g51 (time objects)
    struct {
        DNP3_Time           abstime;
        uint32_t            interval;
        DNP3_IntervalUnit   unit:8;
    } time;

    // g52v1, g52v2 (delays)
    uint32_t delay;             // always in milliseconds!

    // g90v1 (application id)
    struct {
        char *str;
        size_t len;
    } applid;

    // objects with timestamps (not group 50!)
    struct {
        union {
            DNP3_Flags flags;
            DNP3_CommandEvent cmdev;
            DNP3_Counter ctr;
            DNP3_Analog ana;
        };
        union {
            DNP3_Time abstime;  // ms since 1970-01-01
            uint16_t  reltime;  // ms since "common time-of-occurance" (CTO)
        };
    } timed;

} DNP3_Object;

typedef struct {
    DNP3_Group      group;
    DNP3_Variation  variation;

    size_t      count;          // number of objects
    uint32_t    range_base;     // 0 if unused; only used with rangespecs 0-5
    uint32_t    *indexes;       // NULL if unused
    DNP3_Object *objects;

    // low-level packet info
    uint8_t     prefixcode:4;
    uint8_t     rangespec:4;
} DNP3_ObjectBlock;

typedef struct {
} DNP3_AuthData; // XXX

// requests are messages from master to outstation.
// responses (solicited or unsolicited) are messages from outstation to master.
// they have different parsers but use the same data structure representation:
typedef struct {
    DNP3_AppControl     ac;     // application control octet
    DNP3_FunctionCode   fc;     // function code
    DNP3_IntIndications iin;    // internal indications (response only)

    DNP3_AuthData       *auth;  // aggressive-mode authentication (optional)

    size_t              nblocks;    // number of elements in odata array
    DNP3_ObjectBlock    **odata;
} DNP3_Fragment;


/// PARSERS ///

enum DNP3_TokenType {
    // TT_USER remains the generic (void pointer) case
    TT_DNP3_Fragment = TT_USER+1,
    TT_DNP3_AppControl,
    TT_DNP3_IntIndications,
    TT_DNP3_ObjectBlock,
    TT_DNP3_AuthData,
    TT_DNP3_Object,

    TT_DNP3_Segment,

    TT_DNP3_Frame
};

extern HParser *dnp3_p_app_request;
extern HParser *dnp3_p_app_response;

extern HParser *dnp3_p_transport_segment;

extern HParser *dnp3_p_link_frame;

void dnp3_p_init(void);


/// FORMATTING FOR HUMAN-READABLE OUTPUT ///

// caller must free result on all of the following!
char *dnp3_format_object(DNP3_Group g, DNP3_Variation v, const DNP3_Object o);
char *dnp3_format_oblock(const DNP3_ObjectBlock *ob);
char *dnp3_format_fragment(const DNP3_Fragment *frag);
char *dnp3_format_segment(const DNP3_Segment *seg);
char *dnp3_format_frame(const DNP3_Frame *frame);


#endif // DNP3_H_SEEN
