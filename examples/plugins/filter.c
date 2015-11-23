#include <stdio.h>

#include <dnp3hammer/dnp3.h>


// helper
static void output(void *env, const uint8_t *input, size_t len)
{
    fwrite(input, 1, len, (FILE *)env); // XXX loop; use write(2)?
}

void output_frame(void *env, const DNP3_Frame *frame,
                  const uint8_t *buf, size_t len)
{
    if(frame->func == DNP3_UNCONFIRMED_USER_DATA ||
       frame->func == DNP3_CONFIRMED_USER_DATA) {
        return;
    }

    output(env, buf, len);
}

void output_fragment(void *env, const DNP3_Fragment *fragment,
                     const uint8_t *buf, size_t len)
{
    output(env, buf, len);
}
