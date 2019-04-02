//
// Created by pauli on 5/20/17.
//
/******************************************************************************
    Copyright (C) 2017 by Vidpresso Inc.
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "vp_obs_video_shmem_source.h"
#include "sharedmem_consumer.h"
#include <obs.h>
#include <stdio.h>
#include <pthread.h>
#include <util/threading.h>
#include <util/bmem.h>
#include <util/threading.h>
#include <util/platform.h>
#include "image_utils.h"
#include "sharedmem_consumer.h"
#include "vp_render_logger.h"


const char *g_vpObsVideo_shmemFileName = NULL;

static void writeDebugPNG(long frameIndex, int w, int h, uint8_t *buf, size_t srcRowBytes)
{
    char path[512];
    snprintf(path, 511, "/tmp/vp_encoder_output/vp_encoder_debug_%04ld.png", frameIndex);

    writePNG(path, w, h, srcRowBytes, buf);

}

static void writeDebugJPEG(long frameIndex, uint64_t ts, int w, int h, uint8_t *buf, size_t srcRowBytes)
{
    // write a JPEG image at half res (resizing is done by loop increment)
    int dstW = w/2;
    int dstH = h/2;
    size_t dstRowBytes = (size_t)dstW*3;

    uint8_t *tempBuf = (uint8_t *)malloc(dstRowBytes * dstH);

    for (int dstY = 0; dstY < dstH; dstY++) {
        uint8_t *s = (uint8_t *)buf + (dstY*2)*srcRowBytes;
        uint8_t *d = tempBuf + dstY*dstRowBytes;
        for (int x = 0; x < dstW; x++) {
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
            d += 3;
            s += 8;  // increment source by two pixels
        }
    }

    double t = (double)ts / 1.0e9;

    char path[512];
    snprintf(path, 511, "/tmp/vp_encoder_output/vp_encoder_debug_%04ld__%.3f.jpg", frameIndex, t);

    writeJPEG(path, dstW, dstH, dstRowBytes, tempBuf, 3, 30);

    free(tempBuf);
}


typedef struct {
    bool         initialized_thread;
    pthread_t    consumer_thread;
    os_event_t   *event;
    obs_source_t *source;
    volatile bool active;
    uint64_t startTime;

    pthread_mutex_t frameBufLock;
    uint8_t *frameBuf;
    int32_t frameW;
    int32_t frameH;
} VPSourceData;


static void *vp_shmem_video_consumer_thread(void *pdata)
{
    VPSourceData *sd = (VPSourceData *)pdata;
    uint64_t last_time = os_gettime_ns();
    const uint64_t sleep_intv = 1000000;
    int64_t lastReadFrameId = 0;
    uint64_t lastWrittenFrameT = 0;

    printf("%s starting\n", __func__);

    //uint64_t audio_ts_offset_ns = (uint64_t)(0.2 * 1.0e9);

    while (os_event_try(sd->event) == EAGAIN) {
        uint64_t prevT = last_time;

        if ( !os_sleepto_ns(last_time += sleep_intv))
            last_time = os_gettime_ns();

        //uint64_t tdiff = last_time - prevT;

        if ( !os_atomic_load_bool(&sd->active))
            continue;

        pthread_mutex_lock(&sd->frameBufLock);

        VPConduitSharedMemData *shmData = vpconduit_shm_lock();
        //printf("... %s locked shm data %p\n", __func__, shmData);
        if ( !shmData) {
            pthread_mutex_unlock(&sd->frameBufLock);
            continue;
        }

        uint64_t ts = os_gettime_ns();
        double timeSinceLastFrame = (double)(ts - lastWrittenFrameT) / 1.0e9;
        bool didUpdateFrameBuf = false;

        int64_t magic = SHAREDMEM_MAGIC_NUM;
        if (0 != memcmp(&magic, &(shmData->magic), 8)) {
            printf("** %s: invalid magic num\n", __func__);
        }
        else if (shmData->msgId > lastReadFrameId || timeSinceLastFrame > 1.0/15.0) {
            //printf(".. writing video frame: reading frame %ld, size %d*%d, audiosize %d, time since last %.3f ms (%.6f)\n",
            //       shmData->msgId, shmData->w, shmData->h, shmData->audioDataSize, timeSinceLastFrame*1000.0, (double)lastWrittenFrameT/1.0e9);

            // copy into thread local framebuf
            size_t w = (size_t) std::min(sd->frameW, shmData->w);
            size_t h = (size_t) std::min(sd->frameH, shmData->h);
            size_t rowBytes = w*4;
            uint8_t *srcBuf = (uint8_t *)shmData->data;
            uint8_t *dstBuf = (uint8_t *)sd->frameBuf;
            for (size_t y = 0; y < h; y++) {
                uint8_t *src = srcBuf + rowBytes*y;
                uint8_t *dst = dstBuf + rowBytes*y;

                //memcpy(dst, src, rowBytes);

                // swap RGBA->BGRA byte order.
                // lobster needs this for whatever reason
                const uint32_t *src32 = (uint32_t *)src;
                uint32_t *dst32 = (uint32_t *)dst;
                const size_t rowInts = w;
                for (size_t x = 0; x < rowInts; x++) {
                    uint32_t v = *src32++;

                    uint32_t r = v & 0xff;
                    uint32_t g = (v >> 8) & 0xff;
                    uint32_t b = (v >> 16) & 0xff;
                    uint32_t a = 255;  // always set alpha to full since we're encoding to video

                    v = (a << 24) | (r << 16) | (g << 8) | (b);

                    *dst32++ = v;
                }
            }
            didUpdateFrameBuf = true;
            lastReadFrameId = shmData->msgId;
            lastWrittenFrameT = ts;

#if 0
            uint w = (uint)shmData->w;
            uint h = (uint)shmData->h;
            int rowBytes = w*4;
            uint8_t *buf = (uint8_t *)shmData->data;
            enum video_format obsFormat = VIDEO_FORMAT_BGRA;
            struct obs_source_frame obsFrame;
            obsFrame.data[0] = buf;
            obsFrame.linesize[0] = (uint32_t)rowBytes;
            obsFrame.width    = w;
            obsFrame.height   = h;
            obsFrame.format   = obsFormat;
            obsFrame.timestamp = ts;



#endif
        }
        vpconduit_shm_unlock();

        if (didUpdateFrameBuf) {
            uint32_t rowBytes = (uint32_t)sd->frameW * 4;
            enum video_format obsFormat = VIDEO_FORMAT_BGRA;
            struct obs_source_frame obsFrame;
            obsFrame.data[0] = sd->frameBuf;
            obsFrame.linesize[0] = rowBytes;
            obsFrame.width    = (uint32_t)sd->frameW;
            obsFrame.height   = (uint32_t)sd->frameH;
            obsFrame.format   = obsFormat;
            obsFrame.timestamp = ts;

            if (0) {
                //writeDebugJPEG(lastReadFrameId, ts, w, h, buf, rowBytes);
                writeDebugPNG(lastReadFrameId, sd->frameW, sd->frameH, sd->frameBuf, rowBytes);
            }

            if (os_atomic_load_bool(&sd->active))
                obs_source_output_video(sd->source, &obsFrame);

            if (g_vpRenderLogger) {
                char msg[512];
                snprintf(msg, 511, "video frame %ld, size %d*%d, time since last %.3f ms",
                         shmData->msgId, shmData->w, shmData->h, timeSinceLastFrame*1000.0);
                g_vpRenderLogger->writeText(VPRenderLogger::VP_RENDERLOG_VIDEO, msg);
            }
        }

        pthread_mutex_unlock(&sd->frameBufLock);
    }

    printf("%s ended\n", __func__);

    return NULL;
}




static const char *vp_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return "Vidpresso Live Video";
}

static void *vp_create(obs_data_t *settings, obs_source_t *source)
{
    printf("%s started\n", __func__);

    g_vpRenderLogger->writeText(VPRenderLogger::VP_RENDERLOG_STATUS, "vp video source create");

    if ( !g_vpObsVideo_shmemFileName || !vpconduit_shm_open_consumer(g_vpObsVideo_shmemFileName)) {
        printf("** %s failed: shmem file name is %s, could not open\n", __func__, g_vpObsVideo_shmemFileName);
        return NULL;
    }

    int w = (int)obs_data_get_int(settings, "w");
    int h = (int)obs_data_get_int(settings, "h");

    VPSourceData *sd = (VPSourceData *) bzalloc(sizeof(VPSourceData));

    sd->source = source;
    os_atomic_set_bool(&sd->active, true);
    sd->startTime = os_gettime_ns();

    sd->frameW = (w > 0) ? w : 1280;
    sd->frameH = (h > 0) ? h : 720;
    sd->frameBuf = (uint8_t *) bmalloc(sd->frameW * 4 * sd->frameH);

    pthread_mutex_init_value(&sd->frameBufLock);

    if (os_event_init(&sd->event, OS_EVENT_TYPE_MANUAL) != 0)
        goto fail;

    printf("%s will start thread\n", __func__);

    if (pthread_create(&sd->consumer_thread, NULL, vp_shmem_video_consumer_thread, sd) != 0)
        goto fail;

    sd->initialized_thread = true;

    printf("%s finished\n", __func__);

    g_vpRenderLogger->writeText(VPRenderLogger::VP_RENDERLOG_STATUS, "vp video source finished.");

    return sd;

    fail:
    return NULL;
}

static void vp_destroy(void *data)
{
    printf("%s started\n", __func__);

    VPSourceData *sd = (VPSourceData *)data;
    if (sd) {
        os_atomic_set_bool(&sd->active, false);

        if (sd->initialized_thread) {
            printf("...signaling vp shmem thread to stop...\n");
            void *ret;
            os_event_signal(sd->event);
            pthread_join(sd->consumer_thread, &ret);
        }

        os_event_destroy(sd->event);

        pthread_mutex_lock(&sd->frameBufLock);
        if (sd->frameBuf) {
            bfree(sd->frameBuf), sd->frameBuf = NULL;
        }
        pthread_mutex_unlock(&sd->frameBufLock);

        bfree(sd);
    }

    printf("%s finished\n", __func__);
}

static void vp_update(void *data, obs_data_t *settings)
{
    VPSourceData *sd = (VPSourceData *)data;
    if ( !sd) return;

    int w = (int)obs_data_get_int(settings, "w");
    int h = (int)obs_data_get_int(settings, "h");

    printf("%s, video size %d * %d\n", __func__, w, h);

    pthread_mutex_lock(&sd->frameBufLock);

    if (sd->frameBuf) bfree(sd->frameBuf);

    sd->frameW = (w > 0) ? w : 1280;
    sd->frameH = (h > 0) ? h : 720;
    sd->frameBuf = (uint8_t *)bmalloc(sd->frameW * 4 * sd->frameH);

    pthread_mutex_unlock(&sd->frameBufLock);
}



struct obs_source_info vp_source_info = {
        .id             = VP_VIDEO_SOURCE_OBS_ID,
        .type           = OBS_SOURCE_TYPE_INPUT,
        .output_flags   = OBS_SOURCE_ASYNC_VIDEO | /*OBS_SOURCE_AUDIO |*/ OBS_SOURCE_DO_NOT_DUPLICATE,
        .get_name       = vp_get_name,
        .create         = vp_create,
        .destroy        = vp_destroy
};


void vp_obs_video_source_register(VPObsVideoSourceDataCallback dataCb, void *userData)
{
    //g_dataCb = dataCb;
    //g_dataCbUserData = userData;

    // this needs to be here (not in initializer), or clang will complain in C++ mode
    vp_source_info.update = vp_update;

    obs_register_source(&vp_source_info);
}
