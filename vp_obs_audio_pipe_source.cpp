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

#include "vp_obs_audio_pipe_source.h"
#include "vp_audio_pipe_consumer.h"
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <util/bmem.h>
#include <util/threading.h>
#include <util/platform.h>
#include <obs.h>
#include "vp_render_logger.h"


#ifdef NSEC_PER_SEC
#undef NSEC_PER_SEC
#endif
#ifdef USEC_PER_SEC
#undef USEC_PER_SEC
#endif
#ifdef NSEC_PER_USEC
#undef NSEC_PER_USEC
#endif
#ifdef NSEC_PER_MSEC
#undef NSEC_PER_MSEC
#endif
#define NSEC_PER_SEC 1000000000ull
#define NSEC_PER_MSEC 1000000ull
#define USEC_PER_SEC 1000000ull
#define NSEC_PER_USEC 1000ull


#define WRITE_AUDIO_DEBUG_FILE 0


const char *g_vpObsAudio_pipeFileName = NULL;


typedef struct VPSourceData {
    bool         initialized_thread;
    pthread_t    driver_consumer_thread;
    os_event_t   *event;
    obs_source_t *source;
    volatile bool active;
    uint64_t startTime;

    VPAudioPipeConsumer *audioPipeConsumer;
} VPSourceData;


static inline uint64_t samples_to_ns(size_t frames, uint_fast32_t rate)
{
    return frames * NSEC_PER_SEC / rate;
}

static inline uint64_t get_sample_time(size_t frames, uint_fast32_t rate)
{
    return os_gettime_ns() - samples_to_ns(frames, rate);
}


#define STARTUP_TIMEOUT_NS (500 * NSEC_PER_MSEC)



static void *vp_audio_driver_consumer_thread(void *pdata)
{
    VPSourceData *sd = (VPSourceData *)pdata;
    uint64_t last_time = os_gettime_ns();
    uint64_t first_ts = 0;
    int64_t totalPackets = 0;
    int64_t totalFrames = 0;

    const long bufSize = 40000;
    int16_t buf[bufSize];


    FILE *debugRecFile = NULL;
#if WRITE_AUDIO_DEBUG_FILE
    debugRecFile = fopen("/tmp/vpaudiorec_streamer.raw", "wb");
#endif

    while (os_event_try(sd->event) == EAGAIN) {
        if (!os_sleepto_ns(last_time += 50000))
            last_time = os_gettime_ns();

        size_t numSamplesRead = sd->audioPipeConsumer->readSInt16AudioData(buf, bufSize);
        size_t numFrames = numSamplesRead / 2;

        if (numFrames > 0) {
            if (debugRecFile) {
                fwrite(buf, 1, numFrames*2*sizeof(int16_t), debugRecFile);
            }

            struct obs_source_audio data;
            //data.data[0] = (const uint8_t *)sharedMem->data;
            data.data[0] = (const uint8_t *)buf;
            data.speakers = SPEAKERS_STEREO;
            data.frames = (int)numFrames;
            data.samples_per_sec = VPAUDIOSAMPLERATE;
            data.format = AUDIO_FORMAT_16BIT;
            data.timestamp = get_sample_time(data.frames, data.samples_per_sec);

            if ( !first_ts)
                first_ts = data.timestamp + STARTUP_TIMEOUT_NS;

            if (data.timestamp > first_ts) {
                if (os_atomic_load_bool(&sd->active))
                    obs_source_output_audio(sd->source, &data);
            }

            totalPackets++;
            totalFrames += data.frames;

            if (g_vpRenderLogger) {
                char msg[512];
                snprintf(msg, 511, "audio packet %ld, samples in packet %ld, timestamp %lu",
                         totalPackets, numSamplesRead, data.timestamp);
                g_vpRenderLogger->writeText(VPRenderLogger::VP_RENDERLOG_AUDIO, msg);
            }
        }
    }


    if (debugRecFile) fclose(debugRecFile);

    return NULL;
}



/* ------------------------------------------------------------------------- */

static const char *vp_audio_source_getname(void *unused)
{
    return "Vidpresso Audio Pipe";
}

static void vp_audio_source_destroy(void *data)
{
    printf("%s started\n", __func__);

    struct VPSourceData *sd = (VPSourceData *)data;

    if (sd) {
        os_atomic_set_bool(&sd->active, false);

        if (sd->initialized_thread) {
            printf("...signaling vp audio thread to stop...\n");
            void *ret;
            os_event_signal(sd->event);
            pthread_join(sd->driver_consumer_thread, &ret);
        }

        os_event_destroy(sd->event);

        bfree(sd);
    }

    printf("%s finished\n", __func__);
}

static void *vp_audio_source_create(obs_data_t *settings,
                                    obs_source_t *source)
{
    UNUSED_PARAMETER(settings);

    g_vpRenderLogger->writeText(VPRenderLogger::VP_RENDERLOG_STATUS, "vp audio source create");

    if ( !g_vpObsAudio_pipeFileName) {
        printf("** %s failed: audio pipe file not set, could not open\n", __func__);
        return NULL;
    }
    std::string pipeFile = g_vpObsAudio_pipeFileName;

    struct VPSourceData *sd = (VPSourceData *)bzalloc(sizeof(struct VPSourceData));
    sd->source = source;
    sd->startTime = os_gettime_ns();
    sd->audioPipeConsumer = new VPAudioPipeConsumer(pipeFile);

    os_atomic_set_bool(&sd->active, true);

    if (os_event_init(&sd->event, OS_EVENT_TYPE_MANUAL) != 0)
        goto fail;

    if (pthread_create(&sd->driver_consumer_thread, NULL, vp_audio_driver_consumer_thread, sd) != 0)
        goto fail;

    sd->initialized_thread = true;

    g_vpRenderLogger->writeText(VPRenderLogger::VP_RENDERLOG_STATUS, "vp audio source create finished.");

    return sd;

    fail:
    printf("** %s: failed\n", __func__);
    vp_audio_source_destroy(sd);
    return NULL;
}



struct obs_source_info vp_audio_source_info = {
        .id           = VP_AUDIO_SOURCE_OBS_ID,
        .type         = OBS_SOURCE_TYPE_INPUT,
        .output_flags = OBS_SOURCE_AUDIO,
        .get_name     = vp_audio_source_getname,
        .create       = vp_audio_source_create,
        .destroy      = vp_audio_source_destroy,
};

void vp_audio_source_register() {
    obs_register_source(&vp_audio_source_info);
}


