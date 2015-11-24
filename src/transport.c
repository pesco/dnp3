
#include <dnp3hammer/dnp3.h>

#include <hammer/hammer.h>
#include <hammer/glue.h>


HParser *dnp3_p_transport_segment;

static HParsedToken *act_segment(const HParseResult *p, void *user)
{
    DNP3_Segment *s = H_ALLOC(DNP3_Segment);

    s->fin = H_FIELD_UINT(0, 0);
    s->fir = H_FIELD_UINT(0, 1);
    s->seq = H_FIELD_UINT(0, 2);

    HCountedArray *a = H_FIELD_SEQ(1);
    s->len = a->used;
    s->payload = h_arena_malloc(p->arena, s->len);
    assert(s->payload != NULL);
    for(size_t i=0; i<s->len; i++) {
        s->payload[i] = H_CAST_UINT(a->elements[i]);
    }

    return H_MAKE(DNP3_Segment, s);
}

void dnp3_p_init_transport(void)
{
    H_RULE(bit,     h_bits(1, false));
    H_RULE(byte,    h_uint8());

    H_RULE(fir,     bit);
    H_RULE(fin,     bit);
    H_RULE(seqno,   h_bits(6, false));
    H_RULE(hdr,     h_sequence(fin, fir, seqno, NULL));     // big-endian
    
    H_ARULE(segment, h_sequence(hdr, h_many(byte), NULL));
        // XXX is there a minimum number of bytes in the transport payload?

    dnp3_p_transport_segment = segment;
}
