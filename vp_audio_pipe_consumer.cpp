//
// Created by pauli on 6/9/17.
//

#include "vp_audio_pipe_consumer.h"
#include "time_utils.h"
#include <iomanip>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include "vp_render_logger.h"


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


VPAudioPipeConsumer::VPAudioPipeConsumer(std::string srcAudioPipePath)
{
    m_audioRingBuffer = (VPAudioRingBuffer *)calloc(sizeof(VPAudioRingBuffer), 1);
    m_audioRingBuffer->bufferSize = 1024*1024;
    m_audioRingBuffer->data = (VPAUDIOSAMPLETYPE *)malloc(m_audioRingBuffer->bufferSize * sizeof(VPAUDIOSAMPLETYPE));

    if ( !srcAudioPipePath.empty()) {
        m_audioPipeFile = srcAudioPipePath;

        std::thread audioThread([this] { this->audioPipeThreadFunc(); });
        audioThread.detach();
    }

}

void VPAudioPipeConsumer::audioPipeThreadFunc()
{
    std::cout << __func__ << " starting" << std::endl;

    int fd = open(m_audioPipeFile.c_str(), O_RDONLY);
    /*if (fd < 0) {
        std::cout << "** Audio thread: could not open: " << m_audioPipeFile << std::endl;
        return;
    }*/

    FILE *dstFile = NULL;
    if (0) {
        // DEBUG: write audio to file
        dstFile = fopen("/tmp/vpaudio_streamer_pipeconsumer.raw", "w");
    }

    size_t bufSize = 512*1024;
    uint8_t *buf = (uint8_t *) malloc(bufSize);

    const double startT = VPMonotonicTime();
    double lastT = startT;

    long totalBytesRead = 0;

    // for the PulseAudio pipe, we expect these specs (shown by the listing from "pacmd list-sinks"):
    //                                           -->    sample spec: s16le 2ch 48000Hz
    const long samplesPerFrame = 512;
    const long bytesPerSample = 2 * 2;  // 2 channels, 2 bytes each
    const long bytesPerFrame = bytesPerSample * samplesPerFrame;
    const long sampleRate = VPAUDIOSAMPLERATE;
    const double frameIntv = (double)samplesPerFrame / sampleRate;

    long numWarnsAboutOpen = 0;

    while (1) {
        std::cout << std::flush;

        lastT += frameIntv;
        if ( !VPMonotonicTimeSleepTo(lastT))
            lastT = VPMonotonicTime();

        if (fd < 0) {
            if ((fd = open(m_audioPipeFile.c_str(), O_RDONLY)) < 0) {
                if (++numWarnsAboutOpen < 10) {
                    std::cout << __func__ << " could not open pipe, probably audio isn't playing yet" << std::endl;
                } else if (numWarnsAboutOpen == 10) {
                    std::cout << __func__ << " could not open pipe (will not warn anymore)" << std::endl;
                }
                VPMonotonicTimeSleepTo(lastT + 1.0);
                continue;
            } else {
                std::cout << __func__ << " did open pipe: " << m_audioPipeFile << std::endl;
            }
        }

        ssize_t bytesRead = read(fd, buf, bytesPerFrame);
        if (bytesRead < 0) {
            std::cout << __func__ << " read() failed with error " << errno << std::endl;
            break;
        }
        if (bytesRead == 0) {
            std::cout << __func__ << " no data in pipe file" << std::endl;
            continue;
        }
        totalBytesRead += bytesRead;

        std::lock_guard<std::mutex> guard(m_audioMutex);

        vpAudioRingBufferWrite(m_audioRingBuffer, bytesRead / sizeof(VPAUDIOSAMPLETYPE), (VPAUDIOSAMPLETYPE *)buf);

        //printf("pipeconsumer at t %f: total %ld, audio read %ld bytes, unconsumed %ld\n", VPMonotonicTime()-startT, totalBytesRead, bytesRead, m_audioRingBuffer->unconsumed);

        if (dstFile) { // DEBUG: write to file
            //std::cout << "... audio thread read " << bytesRead << " bytes" << std::endl;
            fwrite(buf, 1, bytesRead, dstFile);
        }

        if (0 && g_vpRenderLogger) {
            char msg[512];
            snprintf(msg, 511, "pipeconsumer read: samples %ld, total %ld",
                     bytesRead, totalBytesRead);
            g_vpRenderLogger->writeText(VPRenderLogger::VP_RENDERLOG_AUDIO, msg);
        }
    }

    free(buf);
    close(fd);
    if (dstFile) fclose(dstFile);
    std::cout << __func__ << " finished" << std::endl;
    std::cout << std::flush;
}


size_t VPAudioPipeConsumer::readSInt16AudioData(int16_t *audioBuf, size_t audioBufSize)
{
    std::lock_guard<std::mutex> guard(m_audioMutex);

    long numAudioSamples = vpAudioRingBufferReadAvailable(m_audioRingBuffer, audioBufSize, audioBuf);

    //printf("read %ld samples\n", numAudioSamples);

    return (size_t)numAudioSamples;
}
