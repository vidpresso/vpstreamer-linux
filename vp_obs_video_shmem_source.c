//
// Created by pauli on 5/20/17.
//

#include "vp_obs_video_shmem_source.h"
#include "sharedmem_consumer.h"
#include <obs.h>
#include <stdio.h>
#include <pthread.h>
#include <util/threading.h>
#include <util/bmem.h>
#include <util/threading.h>
#include <util/platform.h>

#include "sharedmem_consumer.h"


const char *g_vpObsVideo_shmemFileName = NULL;


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

    int64_t frameIdx;
    double lastWrittenFrameT;
} VPSourceData;


static void *vp_shmem_video_consumer_thread(void *pdata)
{
    VPSourceData *sd = pdata;
    uint64_t last_time = os_gettime_ns();
    const uint64_t sleep_intv = 1000000;
    int64_t lastReadFrameId = 0;

    printf("%s starting\n", __func__);

    uint64_t audio_ts_offset_ns = (uint64_t)(0.2 * 1.0e9);

    while (os_event_try(sd->event) == EAGAIN) {
        uint64_t prevT = last_time;

        if ( !os_sleepto_ns(last_time += sleep_intv))
            last_time = os_gettime_ns();

        uint64_t tdiff = last_time - prevT;

        if ( !os_atomic_load_bool(&sd->active))
            continue;

        VPConduitSharedMemData *shmData = vpconduit_shm_lock();
        //printf("... %s locked shm data %p\n", __func__, shmData);
        if ( !shmData)
            continue;

        int64_t magic = SHAREDMEM_MAGIC_NUM;
        if (0 != memcmp(&magic, &(shmData->magic), 8)) {
            printf("** %s: invalid magic num\n", __func__);
        }
        else if (shmData->msgId > lastReadFrameId) {
            printf(".. %s: reading frame %ld, size %d*%d, audiosize %d\n", __func__,
                   shmData->msgId, shmData->w, shmData->h, shmData->audioDataSize);

            lastReadFrameId = shmData->msgId;

            uint64_t ts = os_gettime_ns();

            uint w = (uint)shmData->w;
            uint h = (uint)shmData->h;
            int rowBytes = w*4;
            uint8_t *buf = (uint8_t *)shmData->data;
            enum video_format obsFormat = VIDEO_FORMAT_BGRA;
            struct obs_source_frame obsFrame = {
                    .data     = { [0] = buf },
                    .linesize = { [0] = rowBytes },
                    .width    = w,
                    .height   = h,
                    .format   = obsFormat,
                    .timestamp = ts
            };

            obs_source_output_video(sd->source, &obsFrame);

            size_t audioNumSamples = shmData->audioDataSize / sizeof(int16_t);
            if (audioNumSamples > 0) {
                struct obs_source_audio audioData;
                audioData.data[0] = (const uint8_t *)(shmData->data + rowBytes*h);
                audioData.frames = (uint)audioNumSamples / 2;  // 2 channels, so each frame is 2 samples
                audioData.speakers = SPEAKERS_STEREO;
                audioData.samples_per_sec = 44100;
                audioData.timestamp = ts + audio_ts_offset_ns;
                audioData.format = AUDIO_FORMAT_16BIT;
                obs_source_output_audio(sd->source, &audioData);

                /*
                const long samplesPerFrame = 512;
                const long bytesPerSample = 2 * 2;  // 2 channels, 2 bytes each
                const long bytesPerFrame = bytesPerSample * samplesPerFrame;
                const long sampleRate = 44100;
                const uint64_t frameIntv_ns = (uint64_t)(((double)samplesPerFrame / sampleRate) * 1.0e9);

                long framesInThisBuf = audioNumSamples / samplesPerFrame;
                printf("... audio frames: %ld (%d bytes), frameintv %lld\n", framesInThisBuf, shmData->audioDataSize, frameIntv_ns);

                if (audio_ts == 0) {
                    audio_ts = ts - framesInThisBuf*frameIntv_ns;
                    printf(" .... resetting audio timestamp at %lld\n", audio_ts);
                }

                int16_t *audioData = (int16_t *)(shmData->data + shmData->);

                for (long i = 0; i < framesInThisBuf; i++) {
                    struct obs_source_audio audioData;
                    audioData.data[0] = ;
                    audioData.frames = audioNumSamples / 2;  // 2 channels, so each frame is 2 samples
                    audioData.speakers = SPEAKERS_STEREO;
                    audioData.samples_per_sec = 44100;
                    audioData.timestamp = audio_ts;
                    audioData.format = AUDIO_FORMAT_16BIT;
                    obs_source_output_audio(sd->source, &audioData);

                    audio_ts += frameIntv_ns;
                }
                 */
            }
        }

        vpconduit_shm_unlock();
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

    if ( !g_vpObsVideo_shmemFileName || !vpconduit_shm_open_consumer(g_vpObsVideo_shmemFileName)) {
        printf("** %s failed: shmem file name is %s, could not open\n", __func__, g_vpObsVideo_shmemFileName);
        return NULL;
    }

    int w = (int)obs_data_get_int(settings, "w");
    int h = (int)obs_data_get_int(settings, "h");

    VPSourceData *sd = bzalloc(sizeof(VPSourceData));

    sd->source = source;
    os_atomic_set_bool(&sd->active, true);
    sd->startTime = os_gettime_ns();

    sd->frameW = (w > 0) ? w : 1280;
    sd->frameH = (h > 0) ? h : 720;
    sd->frameBuf = bmalloc(sd->frameW * 4 * sd->frameH);

    pthread_mutex_init_value(&sd->frameBufLock);

    if (os_event_init(&sd->event, OS_EVENT_TYPE_MANUAL) != 0)
        goto fail;

    printf("%s will start thread\n", __func__);

    if (pthread_create(&sd->consumer_thread, NULL, vp_shmem_video_consumer_thread, sd) != 0)
        goto fail;

    sd->initialized_thread = true;

    printf("%s finished\n", __func__);

    return sd;

    fail:
    return NULL;
}

static void vp_destroy(void *data)
{
    printf("%s started\n", __func__);

    VPSourceData *sd = data;
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
    VPSourceData *sd = data;
    if ( !sd) return;

    int w = (int)obs_data_get_int(settings, "w");
    int h = (int)obs_data_get_int(settings, "h");

    //VDPLog(@"%s, size %d * %d", __func__, w, h);

    pthread_mutex_lock(&sd->frameBufLock);

    if (sd->frameBuf) bfree(sd->frameBuf);

    sd->frameW = (w > 0) ? w : 1280;
    sd->frameH = (h > 0) ? h : 720;
    sd->frameBuf = bmalloc(sd->frameW * 4 * sd->frameH);

    pthread_mutex_unlock(&sd->frameBufLock);
}

struct obs_source_info vp_source_info = {
        .id             = VP_VIDEO_SOURCE_OBS_ID,
        .type           = OBS_SOURCE_TYPE_INPUT,
        .output_flags   = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE,
        .get_name       = vp_get_name,
        .create         = vp_create,
        .destroy        = vp_destroy,
        .update         = vp_update,
};


void vp_obs_video_source_register(VPObsVideoSourceDataCallback dataCb, void *userData)
{
    //g_dataCb = dataCb;
    //g_dataCbUserData = userData;

    obs_register_source(&vp_source_info);
}
