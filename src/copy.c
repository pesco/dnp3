#include <dnp3hammer.h>
#include <string.h>

DNP3_Frame *dnp3_copy_frame(HAllocator *mm, const DNP3_Frame *frame)
{
    DNP3_Frame *r = mm->alloc(mm, sizeof(DNP3_Frame));
    if(!r) return NULL;

    *r = *frame;
    if(r->payload) {
        r->payload = mm->alloc(mm, r->len);
        if(!r->payload) {
            mm->free(mm, r);
            return NULL;
        }
        memcpy(r->payload, frame->payload, r->len);
    }

    return r;
}

void dnp3_free_frame(HAllocator *mm, DNP3_Frame *frame)
{
    mm->free(mm, frame->payload);
    mm->free(mm, frame);
}

DNP3_Segment *dnp3_copy_segment(HAllocator *mm, const DNP3_Segment *segment)
{
    DNP3_Segment *r = mm->alloc(mm, sizeof(DNP3_Segment));
    if(!r) return NULL;

    *r = *segment;
    if(r->payload) {
        r->payload = mm->alloc(mm, r->len);
        if(!r->payload) {
            mm->free(mm, r);
            return NULL;
        }
        memcpy(r->payload, segment->payload, r->len);
    }

    return r;
}

void dnp3_free_segment(HAllocator *mm, DNP3_Segment *segment)
{
    mm->free(mm, segment->payload);
    mm->free(mm, segment);
}

DNP3_Fragment *dnp3_copy_fragment(HAllocator *mm, const DNP3_Fragment *fragment)
{
    DNP3_Fragment *r;

    #define ALLOC(VAR,SIZE) do {    \
        size_t size = (SIZE);       \
        VAR = mm->alloc(mm, size);  \
        if(!VAR) goto fail;         \
    } while(0)

    ALLOC(r, sizeof(DNP3_Fragment));
    *r = *fragment;
    r->auth = NULL;
    r->odata = NULL;
    if(fragment->auth) {
        ALLOC(r->auth, sizeof(DNP3_AuthData));
        *r->auth = *fragment->auth; // XXX ?
    }
    if(fragment->odata) {
        size_t len = sizeof(DNP3_ObjectBlock *) * r->nblocks;
        ALLOC(r->odata, len);
        memset(r->odata, 0, len);
        for(size_t i=0; i<fragment->nblocks; i++) {
            const DNP3_ObjectBlock *ob = fragment->odata[i];
            ALLOC(r->odata[i], sizeof(DNP3_ObjectBlock));
            *r->odata[i] = *ob;
            r->odata[i]->indexes = NULL;
            r->odata[i]->objects = NULL;
            if(ob->indexes) {
                size_t len = (sizeof *ob->indexes) * ob->count;
                ALLOC(r->odata[i]->indexes, len);
                memcpy(r->odata[i]->indexes, ob->indexes, len);
            }
            if(ob->objects) {
                size_t len = (sizeof *ob->objects) * ob->count;
                ALLOC(r->odata[i]->objects, len);

                // XXX is g90v1 the only deep-copy case in DNP3_Object!?
                if(ob->group == DNP3_GROUP_APPL &&
                   ob->variation == DNP3_VARIATION_APPL_ID) {
                    memset(r->odata[i]->objects, 0, len);
                    for(size_t j=0; j<ob->count; j++) {
                        size_t len = ob->objects[j].applid.len;
                        ALLOC(r->odata[i]->objects[j].applid.str, len);
                        r->odata[i]->objects[j].applid.len = len;
                        memcpy(r->odata[i]->objects[j].applid.str,
                               ob->objects[j].applid.str, len);
                    }
                } else {
                    memcpy(r->odata[i]->objects, ob->objects, len);
                }
            }
        }
    }

    #undef ALLOC
    return r;

fail:
    dnp3_free_fragment(mm, r);
    return NULL;
}

void dnp3_free_fragment(HAllocator *mm, DNP3_Fragment *f)
{
    if(f) {
        if(f->auth) mm->free(mm, f->auth);
        for(size_t i=0; i<f->nblocks; i++) {
            DNP3_ObjectBlock *ob = f->odata[i];
            if(ob) {
                if(ob->indexes)
                    mm->free(mm, ob->indexes);
                if(ob->objects) {
                    // XXX is g90v1 the only deep-copy case in DNP3_Object!?
                    if(ob->group == DNP3_GROUP_APPL &&
                       ob->variation == DNP3_VARIATION_APPL_ID) {
                        for(size_t j=0; j<ob->count; j++) {
                            if(ob->objects[j].applid.str)
                                mm->free(mm, ob->objects[j].applid.str);
                        }
                    }
                    mm->free(mm, ob->objects);
                }
                mm->free(mm, ob);
            }
        }
        if(f->odata) mm->free(mm, f->odata);
        mm->free(mm, f);
    }
}
