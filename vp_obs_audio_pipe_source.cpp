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


// first seconds of audio data coming in from VPConduit can be very choppy.
// to avoid encoding that, we'll just ignore first seconds and then render a small fade
// on the first buffer.
#define STARTUP_TIMEOUT_NS (2000 * NSEC_PER_MSEC)


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






/* ---- TEST: sinewave ---- */

/* middle C */
static const double SINEWAVE_RATE = 261.63/48000.0;

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

#define M_PI_X2 M_PI*2

static void *sinewave_thread(void *pdata)
{
    VPSourceData *swd = (VPSourceData *)pdata;
    uint64_t last_time = os_gettime_ns();
    uint64_t ts = 0;
    double cos_val = 0.0;
    uint8_t bytes[480];

    while (os_event_try(swd->event) == EAGAIN) {
        if (!os_sleepto_ns(last_time += 10000000))
            last_time = os_gettime_ns();

        if (!os_atomic_load_bool(&swd->active))
            continue;

        for (size_t i = 0; i < 480; i++) {
            cos_val += SINEWAVE_RATE * M_PI_X2;
            if (cos_val > M_PI_X2)
                cos_val -= M_PI_X2;

            double wave = cos(cos_val) * 0.5;
            bytes[i] = (uint8_t)((wave+1.0)*0.5 * 255.0);
        }

        struct obs_source_audio data;
        data.data[0] = bytes;
        data.frames = 480;
        data.speakers = SPEAKERS_MONO;
        data.samples_per_sec = 48000;
        data.timestamp = ts;
        data.format = AUDIO_FORMAT_U8BIT;
        obs_source_output_audio(swd->source, &data);

        ts += 10000000;
    }

    return NULL;
}




// ----

static void *vp_audio_driver_consumer_thread(void *pdata)
{
    VPSourceData *sd = (VPSourceData *)pdata;
    uint64_t last_time = os_gettime_ns();
    uint64_t first_ts = 0;
    int64_t totalPackets = 0;
    int64_t totalFrames = 0;
    bool didOutput = false;

    const long bufSize = 40000;
    int16_t buf[bufSize];

    // debugging setup
    double cos_val = 0.0;
    bool renderSineWave = false;
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

            if (renderSineWave) {
                const double mixLevel = 0.8;
                for (size_t i = 0; i < numFrames; i++) {
                    cos_val += SINEWAVE_RATE * M_PI_X2;
                    if (cos_val > M_PI_X2)
                        cos_val -= M_PI_X2;

                    double wave = cos(cos_val) * 0.5;

                    int16_t orig_l = buf[i*2];
                    double mixv = wave * 32767.0 * 0.75;
                    mixv = mixv*mixLevel + orig_l*(1.0 - mixLevel);

                    int16_t v = (int16_t)(mixv);
                    buf[i*2] = v;
                    buf[i*2+1] = v;
                }
            }

            int16_t minV = INT16_MAX;
            int16_t maxV = INT16_MIN;
            ///const double VOL_M = 0.95;
            for (size_t i = 0; i < numFrames; i++) {
                int16_t orig_l = buf[i*2];
                int16_t orig_r = buf[i*2+1];
                if (orig_l < minV) minV = orig_l;
                if (orig_l > maxV) maxV = orig_l;
                if (orig_r < minV) minV = orig_r;
                if (orig_r > maxV) maxV = orig_r;

                // TESTING
                ///buf[i*2] = (int16_t)(orig_l * VOL_M);
                ///buf[i*2+1] = (int16_t)(orig_l * VOL_M);
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
                if ( !didOutput) {
                    for (size_t i = 0; i < numFrames; i++) {
                        int16_t orig_l = buf[i*2];
                        int16_t orig_r = buf[i*2+1];

                        // render a ramp within this buffer so we get a small fade at the start of audio
                        double m = (double)i/numFrames;
                        int16_t l = (int16_t)(orig_l * m);
                        int16_t r = (int16_t)(orig_r * m);

                        buf[i*2] = l;
                        buf[i*2+1] = r;
                    }
                }

                if (os_atomic_load_bool(&sd->active)) {
                    obs_source_output_audio(sd->source, &data);
                    didOutput = true;
                }
            }

            totalPackets++;
            totalFrames += data.frames;

            if (g_vpRenderLogger) {
                char msg[512];
                snprintf(msg, 511, "audio packet %ld, samples in packet %ld, timestamp %lu, audio data min/max %d / %d",
                         totalPackets, numSamplesRead, data.timestamp, minV, maxV);
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
    //if (pthread_create(&sd->driver_consumer_thread, NULL, sinewave_thread, sd) != 0)
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


