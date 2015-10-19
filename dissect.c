#include <dnp3.h>
#include <hammer/hammer.h>
#include <hammer/glue.h>
#include "src/hammer.h"
#include <string.h>

#include <unistd.h>


HParser *dnp3_p_framed_segment;     // a transport segment in a user data frame
HParser *dnp3_p_synced_frame;       // skips bytes until valid frame header
HParser *dnp3_p_assembled_payload;  // discards leading unexpected segments
HParser *dnp3_p_app_message;        // application request or response


static bool validate_dataframe(HParseResult *p, void *user)
{
    DNP3_Frame *frame = H_CAST(DNP3_Frame, p->ast);
    assert(frame);

    // ignore frames with corrupted payloads
    if(frame->len > 0 && !frame->payload)
        return false;

    return (frame->func == DNP3_CONFIRMED_USER_DATA
            || frame->func == DNP3_UNCONFIRMED_USER_DATA);
}

static bool validate_first(HParseResult *p, void *user)
{
    return H_CAST(DNP3_Segment, p->ast)->fir;
}

static bool validate_final(HParseResult *p, void *user)
{
    return H_CAST(DNP3_Segment, p->ast)->fin;
}

static bool validate_equ(HParseResult *p, void *user)
{
    DNP3_Segment *a = H_FIELD(DNP3_Segment, 0);
    DNP3_Segment *b = H_FIELD(DNP3_Segment, 1);

    // a and b must be byte-by-byte identical
    return (a->fir == b->fir &&
            a->fin == b->fin &&
            a->seq == b->seq &&
            a->len == b->len &&
            (a->payload == b->payload ||    // catches NULL case
             memcmp(a->payload, b->payload, a->len) == 0));
}

static bool validate_seq(HParseResult *p, void *user)
{
    DNP3_Segment *a = H_FIELD(DNP3_Segment, 0);
    DNP3_Segment *b = H_FIELD(DNP3_Segment, 1);

    return (b->seq == (a->seq + 1)%64);
}

static HParsedToken *act_payload(const HParseResult *p, void *user)
{
    HCountedArray *a = H_CAST_SEQ(p->ast);
    size_t len = 0;

    // allocate result
    for(size_t i=0; i<a->used; i++) {
        len += H_CAST(DNP3_Segment, a->elements[i])->len;
    }

    // assemble segments
    uint8_t *t = h_arena_malloc(p->arena, len);
    uint8_t *s = t;
    for(size_t i=0; i<a->used; i++) {
        DNP3_Segment *seg = H_CAST(DNP3_Segment, a->elements[i]);
        memcpy(s, seg->payload, seg->len);
        s += seg->len;
    }

    return H_MAKE_BYTES(t, len);
}

extern HAllocator system_allocator; // XXX
static HParser *k_frame(HAllocator *mm__, const HParsedToken *p, void *user)
{
    HAllocator *mres = &system_allocator;
    const HParser *parser = user;
    DNP3_Frame *frame = H_CAST(DNP3_Frame, p);
    HParseResult *res = NULL;

    if(frame->payload || frame->len==0)
        res = h_parse__m(mres, parser, frame->payload, frame->len);

    if(!res)
        return NULL;
    else
        return h_unit__m(mm__, res->ast);
}

void init(void)
{
    dnp3_p_init();

    // XXX should filter/split link layer by address and type

    HParser *sync = h_indirect();
    H_RULE(sync_, h_choice(dnp3_p_link_frame, h_right(h_uint8(), sync), NULL));
        // XXX is it correct to skip one byte looking for the frame start?
    h_bind_indirect(sync, sync_);

    // hook in link layer parser
    // each transport segment arrives in a link-layer frame.
    H_VRULE(dataframe, sync);
    H_RULE (segment,   h_bind(sync,      k_frame, dnp3_p_transport_segment));
    H_RULE (dataseg,   h_bind(dataframe, k_frame, dnp3_p_transport_segment));

    // we can state what makes a valid series of segments, how they assemble,
    // and when/what to discard as erroneous:
    // cf. IEEE 1815-2012 section 8.2

    H_VRULE(first,  dataseg);   // fir set
    H_VRULE(final,  dataseg);   // fin set

    // deduplication
    H_VRULE(equ,    h_sequence(dataseg, dataseg, NULL));
    H_RULE (dup,    h_left(h_and(equ), dataseg));
    H_RULE (seg,    h_right(h_many(dup), dataseg));

    H_RULE (notfir, h_right(h_not(first), seg));
    H_RULE (notfin, h_right(h_not(final), seg));

    // "pre-sequence" = valid seq. numbers, dedup'ed, no spurious fir/fin flags
    H_VRULE(seq,    h_sequence(notfin, notfir, NULL));
    H_RULE (elt,    h_right(h_and(seq), seg));  // ...
    H_RULE (pre_,   h_many(elt));               // last segment not consumed!
    H_RULE (pre,    h_sequence(pre_, seg, NULL));

    #define act_valid h_act_flatten
    H_RULE (single,  h_right(h_and(first), final)); // no deduplication
    H_RULE (multi,   h_sequence(h_and(first), pre_, final, NULL));
    H_ARULE(valid,   h_choice(single, multi, NULL));
    H_RULE (invalid, h_right(h_not(valid), pre));
    H_RULE (garbage, h_many(invalid));
    H_ARULE(payload, h_middle(garbage, valid, garbage));

    H_RULE(message, h_choice(dnp3_p_app_request, dnp3_p_app_response, NULL));

    dnp3_p_framed_segment = segment;
    dnp3_p_assembled_payload = payload;
    dnp3_p_synced_frame = sync;
    dnp3_p_app_message = message;
}


#define BUFLEN 4100 // enough for 2048B over 512 frames or 4096B over 1 frame

int main(int argc, char *argv[])
{
    uint8_t buf[BUFLEN];
    size_t n=0, m, l=0, t=0;

    init();

    // while stdin open, read additional input into buf
    while(n<BUFLEN && (m=read(0, buf+n, BUFLEN-n))) {
        HParseResult *r;

        n += m;

        // parse and print lower layers
        r = h_parse(h_many(dnp3_p_synced_frame), buf+l, n-l); 
        if(r) {
            l += r->bit_length/8;
            assert(r->ast);
            HCountedArray *seq = H_CAST_SEQ(r->ast);
            for(size_t i=0; i<seq->used; i++) {
                DNP3_Frame *frame = H_CAST(DNP3_Frame, seq->elements[i]);
                printf("L> %s\n", dnp3_format_frame(frame));
            }
        }
        r = h_parse(h_many(dnp3_p_framed_segment), buf+t, n-t); 
        if(r) {
            t += r->bit_length/8;
            assert(r->ast);
            HCountedArray *seq = H_CAST_SEQ(r->ast);
            for(size_t i=0; i<seq->used; i++) {
                DNP3_Segment *segment = H_CAST(DNP3_Segment, seq->elements[i]);
                printf("T> %s\n", dnp3_format_segment(segment));
            }
        }

        // try to reassemble sequences of segments
        r = h_parse(h_many(dnp3_p_assembled_payload), buf, n);
        if(r) {
            // flush consumed input
            size_t consumed = r->bit_length/8;
            n -= consumed;
            memmove(buf, buf+consumed, n);
            t=l=0;

            assert(r->ast);
            HCountedArray *seq = H_CAST_SEQ(r->ast);
            for(size_t i=0; i<seq->used; i++) {
                HBytes bytes = H_CAST_BYTES(seq->elements[i]);

                printf("T> reassembled payload:");
                for(size_t i=0; i<bytes.len; i++)
                    printf(" %.2X", (unsigned int)bytes.token[i]);
                printf("\n");

                // try to parse a message
                r = h_parse(dnp3_p_app_message, bytes.token, bytes.len);
                if(r) {
                    DNP3_Fragment *fragment = H_CAST(DNP3_Fragment, r->ast);
                    printf("A> %s\n", dnp3_format_fragment(fragment));
                } else {
                    printf("A> parse error\n");
                }
            }
        }
    }

    if(n>0) {
        if(n>=BUFLEN)
            fprintf(stderr, "input buffer exhausted\n");
        else
            fprintf(stderr, "no parse\n");
        return 1;
    }

    return 0;
}
