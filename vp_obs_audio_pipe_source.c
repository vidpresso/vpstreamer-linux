//
// Created by pauli on 5/20/17.
//

#include "vp_obs_audio_pipe_source.h"
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>


#ifndef MAX
#define MAX(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#endif
#ifndef MIN
#define MIN(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })
#endif



// this ring buffer struct type and associated functions borrowed from the Vidpresso Mac version
//

#define VPAUDIOSAMPLETYPE int16_t

typedef struct VPAudioRingBuffer {

    VPAUDIOSAMPLETYPE *data;
    size_t bufferSize;

    long writerPosition;
    long readerPosition;
    long unconsumed;  // means "written but not yet read"

} VPAudioRingBuffer;

#ifndef VPAUDIORINGBUF_ADVANCE
#define VPAUDIORINGBUF_ADVANCE(rb_, p_, n_) \
                    *(p_) += n_; \
                    if (*(p_) >= rb_->bufferSize) *(p_) = 0;
#endif


static long vpAudioRingBufferReadAvailable(VPAudioRingBuffer *rb, long maxCount, VPAUDIOSAMPLETYPE *dst)
{
    if ( !rb || !dst || maxCount < 1 || rb->unconsumed < 1) return 0;

    long consumed = 0;
    long count = MIN(maxCount, rb->unconsumed);

    if (rb->unconsumed > count) {
        VPAUDIORINGBUF_ADVANCE(rb, &rb->readerPosition, rb->unconsumed - count);
        rb->unconsumed = count;
    }

    long availEnd = rb->bufferSize - rb->readerPosition;
    long n1 = MIN(count, availEnd);
    count -= n1;

    memcpy(dst, rb->data + rb->readerPosition, n1*sizeof(VPAUDIOSAMPLETYPE));
    dst += n1;

    VPAUDIORINGBUF_ADVANCE(rb, &rb->readerPosition, n1);
    consumed += n1;

    if (count > 0) {
        long n2 = MIN(count, rb->bufferSize);
        consumed += n2;

        memcpy(dst, rb->data + rb->readerPosition, n2*sizeof(VPAUDIOSAMPLETYPE));

        VPAUDIORINGBUF_ADVANCE(rb, &rb->readerPosition, n2);
        consumed += n2;
    }
    rb->unconsumed -= consumed;
    return consumed;
}

static void vpAudioRingBufferWrite(VPAudioRingBuffer *rb, long count, VPAUDIOSAMPLETYPE *src) {
    if (!rb || !src || count < 1) return;

    if (rb->unconsumed >= rb->bufferSize - 1) {
        printf("%s: buffer overflowing (no consumer), will reset now\n", __func__);
        rb->unconsumed = 0;
        rb->writerPosition = 0;
        rb->readerPosition = 0;
    }

    long availEnd = rb->bufferSize - rb->writerPosition;
    long n1 = MIN(count, availEnd);
    count -= n1;

    long written = 0;

    memcpy(rb->data + rb->writerPosition, src, n1 * sizeof(VPAUDIOSAMPLETYPE));
    src += n1;
    VPAUDIORINGBUF_ADVANCE(rb, &rb->writerPosition, n1);
    written += n1;

    if (count > 0) {
        long n2 = MIN(count, rb->bufferSize);

        memcpy(rb->data + rb->writerPosition, src, n2 * sizeof(VPAUDIOSAMPLETYPE));

        VPAUDIORINGBUF_ADVANCE(rb, &rb->readerPosition, n2);
        written += n2;
    }

    rb->unconsumed += written;
}



