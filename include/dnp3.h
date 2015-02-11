#ifndef DNP3_H_SEEN
#define DNP3_H_SEEN

#include <hammer/hammer.h>


/// TYPES ///

typedef struct {
    uint8_t fin:1;  // note: ignore on unsolicited responses (treat as 1)!
    uint8_t fir:1;  // note: ignore on unsolicited responses (treat as 1)!
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
    DNP3_GROUP_CROB = 12,
    DNP3_GROUP_PCB = 13,
    DNP3_GROUP_AUTH = 120,
} DNP3_Group;

// variation numbers
typedef enum {
    DNP3_VARIATION_ANY = 0,

    DNP3_VARIATION_PACKED = 1,
    DNP3_VARIATION_FLAGS = 2,

    DNP3_VARIATION_NOTIME = 1,
    DNP3_VARIATION_ABSTIME = 2,
    DNP3_VARIATION_RELTIME = 3,

    DNP3_VARIATION_ALL = 254,

    DNP3_VARIATION_AGGR = 3,
    DNP3_VARIATION_MAC = 9,
} DNP3_Variation;

typedef union {
    // g1v1, g10v1 (binary in- and outputs, packed format)
    uint8_t bit:1;

    // g1v2, g10v2 (binary in- and outputs, with flags)
    struct {
        uint8_t online:1;
        uint8_t restart:1;
        uint8_t comm_lost:1;
        uint8_t remote_forced:1;
        uint8_t local_forced:1;
        uint8_t chatter_filter:1;
        uint8_t state:1;
    } flags;

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
    TT_DNP3_Object
    // XXX ...
};

extern HParser *dnp3_p_app_request;
extern HParser *dnp3_p_app_response;

void dnp3_p_init(void);

// formatting for human-readable output - caller must free result!
char *dnp3_format_object(DNP3_Group g, DNP3_Variation v, const DNP3_Object o);
char *dnp3_format_oblock(const DNP3_ObjectBlock *ob);
char *dnp3_format_fragment(const DNP3_Fragment *req);


#endif // DNP3_H_SEEN
