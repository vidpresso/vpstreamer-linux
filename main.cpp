#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <obs.h>
#include "vp_obs_audio_pipe_source.h"
#include "vp_obs_video_shmem_source.h"
#include <util/platform.h>


#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000ull
#endif


// We don't want streaming to run indefinitely because disk space for recordings is limited.
// The process will self-terminate when this limit is exceeded.
#define MAX_PROCESS_RUNTIME_HOURS 8


template<typename T>
void split(const std::string &s, char delim, T result) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}


static std::string *s_shmemFileName = NULL;
static std::string *s_pidFileName = NULL;
static std::string *s_statusFileName = NULL;
static std::string *s_recordingFileName = NULL;
static uint32_t s_videoW = 1280;
static uint32_t s_videoH = 720;
static double s_audioSyncOffsetInSecs = 0.6;
//static std::string *s_streamUrl = NULL;
//static std::string *s_streamKey = NULL;
static std::vector<std::string> s_streamUrls;
static std::vector<std::string> s_streamKeys;


static std::atomic_bool s_running;
static std::atomic_bool s_streaming;
static std::atomic_bool s_interrupted;


static void writeStatusToFile(const char *msg)
{
    if ( !s_statusFileName) return;

    FILE *fd = fopen(s_statusFileName->c_str(), "w");
    if (fd) {
        size_t len = strlen(msg);
        fwrite(msg, len, 1, fd);
        fclose(fd);
    }
}

static void pidMonitorThreadFunc()
{
    std::cout << __func__ << " starting" << std::endl;

    uint32_t sleepIntv = 3*NSEC_PER_SEC;  // longer initial wait

    uint64_t startT = os_gettime_ns();

    while (1) {
        usleep(sleepIntv);
        sleepIntv = 10*1000;

        struct stat st;
        int res = stat(s_pidFileName->c_str(), &st);

        if (res != 0) {
            // file doesn't exist anymore, so we should terminate
            std::cout << "pidfile has vanished (errno" << errno << "), will interrupt" << std::endl;
            s_interrupted = true;
            break;
        }

        // check that we're not running indefinitely
        double timeElapsed = (double)(os_gettime_ns() - startT) / NSEC_PER_SEC;
        if (timeElapsed > MAX_PROCESS_RUNTIME_HOURS*60*60) {
            std::cout << "Process has been running for more than " <<MAX_PROCESS_RUNTIME_HOURS<< "hours, will exit" << std::endl;
            s_interrupted = true;
            break;
        }
    }

    std::cout << __func__ << " finished" << std::endl;
}
/*
static void commandPipeThreadFunc()
{
    std::cout << __func__ << " starting" << std::endl;

    int fd = open(s_cmdPipeFileName->c_str(), O_RDONLY);
    if (fd < 0) {
        std::cout << __func__ << " failed, could not open: " << *s_cmdPipeFileName << std::endl;
        return;
    }

    const int bufSize = 512;
    uint8_t buf[512];

    while (1) {
        usleep(5*1000);

        ssize_t bytesRead = read(fd, buf, bufSize - 1);
        if (bytesRead < 0) {
            std::cout << __func__ << " read() failed with error " << errno << std::endl;
            break;
        }
        if (bytesRead == 0) {
            continue;
        }
        buf[bytesRead] = 0;  // ensure null-terminated string

        std::cout << "... cmd thread read " << bytesRead << " bytes: " << (const char *)buf << std::endl;

        if (0 == memcmp(buf, "#exit", 5)) {
            s_interrupted = true;
        }
    }

    close(fd);
    std::cout << __func__ << " finished" << std::endl;
}
 */



// obs state
obs_encoder_t *s_aacStreaming;
obs_encoder_t *s_h264Streaming;
obs_source_t *s_vpVideoSource;
obs_source_t *s_vpAudioSource;
obs_output_t *s_fileOutput;

#define MAXSERVICES 8
obs_output_t *s_streamOutputs[MAXSERVICES];
obs_service_t *s_services[MAXSERVICES];
int s_numStreams = 0;


typedef struct {
    long serviceIndex;
} VPObsCallbackData;

static void obsOutputStartCb(void *data, struct calldata *calldata)
{
    VPObsCallbackData *cbData = (VPObsCallbackData *)data;
    char text[256];
    snprintf(text, 255, "output-started_%ld", cbData->serviceIndex);
    writeStatusToFile(text);
}

static void obsOutputStopCb(void *data, struct calldata *calldata)
{
    VPObsCallbackData *cbData = (VPObsCallbackData *)data;
    char text[256];
    snprintf(text, 255, "output-stopped_%ld", cbData->serviceIndex);
    writeStatusToFile(text);
}

static void obsOutputReconnectCb(void *data, struct calldata *calldata)
{
    VPObsCallbackData *cbData = (VPObsCallbackData *)data;
    char text[256];
    snprintf(text, 255, "output-reconnect_%ld", cbData->serviceIndex);
    writeStatusToFile(text);
}

static void obsOutputReconnectSuccessCb(void *data, struct calldata *calldata)
{
    VPObsCallbackData *cbData = (VPObsCallbackData *)data;
    char text[256];
    snprintf(text, 255, "output-reconnect-success_%ld", cbData->serviceIndex);
    writeStatusToFile(text);
}

static void obsLogHandlerCb(int lvl, const char *msg, va_list args, void *p)
{
    printf("obs %d:  ", lvl);
    vprintf(msg, args);
    printf("\n");
}

static void loadObsPlugin(const char *name)
{
    char modulePath[512];
    char moduleDataPath[512];
    //snprintf(modulePath, 511, "/usr/lib/obs-plugins/%s.so", name);
    //snprintf(moduleDataPath, 511, "/usr/share/obs/obs-plugins/%s", name);
    snprintf(modulePath, 511, "../obs-plugins/bin/%s.so", name);
    snprintf(moduleDataPath, 511, "../obs-plugins/data/%s", name);

    obs_module_t *module;
    obs_open_module(&module, modulePath, moduleDataPath);
    if ( !obs_init_module(module)) {
        printf("** %s: could not init module at '%s', %p\n", __func__, modulePath, module);
    }
}

static void initObsLibraries()
{
    obs_startup("en-US", NULL, NULL);

    base_set_log_handler(obsLogHandlerCb, NULL);

    printf("%s startup done\n", __func__);
    /*
    obs_add_module_path([bundlePath stringByAppendingPathComponent:@"Contents/obs-plugins"].UTF8String, [bundlePath stringByAppendingPathComponent:@"Contents/data/obs-plugins/%module%"].UTF8String);

    obs_add_module_path([currentPath stringByAppendingPathComponent:@"obs-plugins"].UTF8String, [currentPath stringByAppendingPathComponent:@"data/obs-plugins/%module%"].UTF8String);

        obs_add_module_path([currentPath stringByAppendingPathComponent:@"../obs-plugins"].UTF8String, [currentPath stringByAppendingPathComponent:@"../data/obs-plugins/%module%"].UTF8String);
    // /data/obs-plugins/%module%
    */
    //obs_load_all_modules();

    loadObsPlugin("obs-ffmpeg");
    loadObsPlugin("obs-outputs");
    loadObsPlugin("obs-x264");
    loadObsPlugin("rtmp-services");

    printf("%s loading standard modules done\n", __func__);

    vp_obs_video_source_register(NULL, NULL);
    //vp_audio_source_register();
    //test_sinewave_register();

    printf("%s done\n", __func__);
}

static void initObsStreaming()
{
    if (s_recordingFileName) {
        s_fileOutput = obs_output_create("ffmpeg_muxer",
                                         "file output", NULL, NULL);
    }

    s_numStreams = std::min(MAXSERVICES, (int)s_streamUrls.size());

    for (int i = 0; i < s_numStreams; i++) {
        if (s_streamUrls[i].length() < 1)
            continue;

        const char *serverUrl = s_streamUrls[i].c_str();
        const char *serverKey = "";
        if (s_streamKeys.size() > i) {
            serverKey = s_streamKeys[i].c_str();
        }

        std::cout << "stream "<<i<<" url: '" << serverUrl << "', key: '" << serverKey <<"'"<< std::endl;

        s_streamOutputs[i] = obs_output_create("rtmp_output",
                                               "vp stream output", NULL, NULL);


        VPObsCallbackData *cbData = (VPObsCallbackData *)calloc(1, sizeof(VPObsCallbackData));
        cbData->serviceIndex = i;
        signal_handler_t *sh;
        sh = obs_output_get_signal_handler(s_streamOutputs[i]);
        signal_handler_connect(sh, "start", obsOutputStartCb, cbData);
        signal_handler_connect(sh, "stop", obsOutputStopCb, cbData);
        signal_handler_connect(sh, "reconnect", obsOutputReconnectCb, cbData);
        signal_handler_connect(sh, "reconnect_success", obsOutputReconnectSuccessCb, cbData);


        s_services[i] = obs_service_create("rtmp_custom",
                                           "vp stream service", NULL, NULL);

        obs_data_t *serviceSettings = obs_data_create();
        obs_data_set_string(serviceSettings, "server", serverUrl);
        obs_data_set_string(serviceSettings, "key", serverKey);
        obs_service_update(s_services[i], serviceSettings);
        obs_data_release(serviceSettings);
    }
    ///exit(1);  // DEBUG

    //std::cout << "stream dest url: " << (s_streamUrl ? *s_streamUrl : "(null)") << std::endl;
/*
    if (s_streamUrl && s_streamKey) {
        const char *serverUrl = s_streamUrl->c_str();  //"rtmp://a.rtmp.youtube.com/live2";
        const char *serverKey = s_streamKey->c_str(); //"dh5q-g412-mbpz-2t0u";
        s_numStreams = 1;

        s_streamOutputs[0] = obs_output_create("rtmp_output",
                                               "vp stream output", NULL, NULL);


        VPObsCallbackData *cbData = (VPObsCallbackData *)calloc(1, sizeof(VPObsCallbackData));
        cbData->serviceIndex = 0;
        signal_handler_t *sh;
        sh = obs_output_get_signal_handler(s_streamOutputs[0]);
        signal_handler_connect(sh, "start", obsOutputStartCb, cbData);
        signal_handler_connect(sh, "stop", obsOutputStopCb, cbData);
        signal_handler_connect(sh, "reconnect", obsOutputReconnectCb, cbData);
        signal_handler_connect(sh, "reconnect_success", obsOutputReconnectSuccessCb, cbData);


        s_services[0] = obs_service_create("rtmp_custom",
                                           "vp stream service", NULL, NULL);

        obs_data_t *serviceSettings = obs_data_create();
        obs_data_set_string(serviceSettings, "server", serverUrl);
        obs_data_set_string(serviceSettings, "key", serverKey);
        obs_service_update(s_services[0], serviceSettings);
        obs_data_release(serviceSettings);
    }
*/
    s_h264Streaming = obs_video_encoder_create("obs_x264",
                                               "simple h264 stream", NULL, NULL);

    s_vpVideoSource = obs_source_create(VP_VIDEO_SOURCE_OBS_ID,
                                        "vidpresso live stream", NULL, NULL);

    if (1) {
        s_aacStreaming = obs_audio_encoder_create("ffmpeg_aac",
                                                  "simple aac", NULL, 0, NULL);

    }



    obs_set_output_source(0, s_vpVideoSource);
}

static void resetObsVideoAndAudio()
{
    int ret;

    struct obs_audio_info oai;
    memset(&oai, 0, sizeof(oai));

    oai.samples_per_sec = 44100;
    oai.speakers = SPEAKERS_STEREO;

    if ((ret = obs_reset_audio(&oai)) != 0) {
        printf("%s warning: reset_audio failed, error %d (this is probably ok!)\n", __func__, ret);
    }

    struct obs_video_info ovi;
    memset(&ovi, 0, sizeof(ovi));

    ovi.graphics_module = "libobs-opengl";

    ovi.fps_num = 30;
    ovi.fps_den = 1;

    ovi.base_width = s_videoW;
    ovi.base_height = s_videoH;

    ovi.output_width = s_videoW;
    ovi.output_height = s_videoH;
    ovi.output_format = VIDEO_FORMAT_RGBA;

    if ((ret = obs_reset_video(&ovi)) != 0) {
        printf("** %s: obs_reset_video failed, error %d\n", __func__, ret);
    } else {
        printf("%s video reset complete\n", __func__);
    }
}

static void updateObsSettings(int videoBitrate, int audioBitrate, int keyIntervalSecs, bool useVBR)
{
    obs_data_t *h264Settings = obs_data_create();
    obs_data_t *aacSettings  = obs_data_create();

    const char *profile = "main";
    const char *preset = "veryfast";
    const char *x264opts = NULL;

    obs_data_set_int(h264Settings, "bitrate", videoBitrate);
    obs_data_set_int(h264Settings, "keyint_sec", keyIntervalSecs);
    obs_data_set_string(h264Settings, "profile", profile);
    obs_data_set_string(h264Settings, "preset", preset);

    if (useVBR) {
        obs_data_set_string(h264Settings, "rate_control", "VBR");
    }

    if (0) {
        obs_data_set_int(h264Settings, "keyint", 5);
    }

    std::cout << "Video settings: bitrate="<<videoBitrate<<" keyinterval="<<keyIntervalSecs<<" profile="<<profile<<" vbr="<<useVBR<<std::endl;

    if (x264opts) {
        obs_data_set_string(h264Settings, "x264opts", x264opts);
        std::cout << "x264 opts string is: " << x264opts << std::endl;
    }

    /*obs_data_set_string(h264Settings, "x264opts",
                        "m8x8dct=1 aq-mode=2 bframes=1 chroma-qp-offset=1 colormatrix=smpte170m deblock=0:0 direct=auto ipratio=1.41 keyint=120 level=3.1 me=hex merange=16 min-keyint=auto mixed-refs=1 no-mbtree=0 partitions=i4x4,p8x8,b8x8 profile=high psy-rd=0.5:0.0 qcomp=0.6 qpmax=51 qpmin=10 qpstep=4 ratetol=10 rc-lookahead=30 ref=1 scenecut=40 subme=5 threads=0 trellis=2 weightb=1 weightp=2");
    */

    obs_data_set_bool(aacSettings, "cbr", true);
    obs_data_set_int(aacSettings, "bitrate", audioBitrate);

    if (s_aacStreaming) {
        std::cout << "Audio settings: bitrate "<<audioBitrate<<std::endl;
    }

    /*
    for (int i = 0; i < _numStreams; i++) {
        obs_service_apply_encoder_settings(_services[i], h264Settings, aacSettings);
    }
    */
    obs_encoder_set_preferred_video_format(s_h264Streaming, VIDEO_FORMAT_NV12);

    obs_encoder_update(s_h264Streaming, h264Settings);
    obs_encoder_update(s_aacStreaming,  aacSettings);

    obs_data_release(h264Settings);
    obs_data_release(aacSettings);

    //obs_source_set_sync_offset(s_vpVideoSource, s_audioSyncOffsetInSecs*NSEC_PER_SEC);
}

static void startObsStreaming()
{
    // connect encoders to obs video and audio
    obs_encoder_set_video(s_h264Streaming, obs_get_video());
    obs_encoder_set_audio(s_aacStreaming, obs_get_audio());


    // set up file output
    time_t now = time(0);
    tm *now_tm = gmtime(&now);
    char dateStr[256];
    strftime(dateStr, sizeof(dateStr), "%Y%m%d_%H%M%S", now_tm);

    if (s_fileOutput) {
        obs_output_set_video_encoder(s_fileOutput, s_h264Streaming);

        if (s_aacStreaming) obs_output_set_audio_encoder(s_fileOutput, s_aacStreaming, 0);

        obs_data_t *fileOutputSettings = obs_data_create();

        obs_data_set_string(fileOutputSettings, "path", s_recordingFileName->c_str());
        //obs_data_set_string(fileOutputSettings, "muxer_settings", mux);
        obs_output_update(s_fileOutput, fileOutputSettings);
        obs_data_release(fileOutputSettings);

        printf("%s: file output path set to %s\n", __func__, s_recordingFileName->c_str());

        obs_output_start(s_fileOutput);
    }

    // set up stream output
    if (s_numStreams > 0) {
        for (int i = 0; i < s_numStreams; i++) {
            obs_output_set_video_encoder(s_streamOutputs[i], s_h264Streaming);

            if (s_aacStreaming) obs_output_set_audio_encoder(s_streamOutputs[i], s_aacStreaming, 0);

            obs_output_set_service(s_streamOutputs[i], s_services[i]);

            obs_output_start(s_streamOutputs[i]);
        }
    }
}

static void stopObsStreaming()
{
    if (s_fileOutput) {
        std::cout << "Stopping file output " << *s_recordingFileName << std::endl;
        obs_output_stop(s_fileOutput);
        usleep(500*1000);
        //obs_output_release(s_fileOutput), s_fileOutput = NULL;
    }
    std::cout << "Streaming stopped" << std::endl;

    obs_set_output_source(0, NULL);
    obs_source_release(s_vpVideoSource), s_vpVideoSource = NULL;
}


static void terminationSignalHandlerCb(int signo) {
    s_interrupted = true;
}

int main(int argc, char* argv[])
{
    initObsLibraries();

    int videoBitrate = 2500;
    int audioBitrate = 128;
    int keyint_secs = 2;
    bool vbr = false;

    for (int i = 1; i < argc; i++) {
        /*if (0 == strcmp("--audiopipe", argv[i]) && i < argc-1) {
            s_audiopipeFileName = new std::string(argv[++i]);
        }*/
        if (0 == strcmp("--shmemfile", argv[i]) && i < argc-1) {
            s_shmemFileName = new std::string(argv[++i]);
        }
        else if (0 == strcmp("--stream-urls", argv[i]) && i < argc-1) {
            //s_streamUrl = new std::string(argv[++i]);

            std::string s(argv[++i]);
            split(s, ',', std::back_inserter(s_streamUrls));

            std::cout << "Got "<<s_streamUrls.size()<<" stream urls" << std::endl;
        }
        else if (0 == strcmp("--stream-keys", argv[i]) && i < argc-1) {
            //s_streamKey = new std::string(argv[++i]);

            std::string s(argv[++i]);
            split(s, ',', std::back_inserter(s_streamKeys));

            std::cout << "Got "<<s_streamKeys.size()<<" stream keys" << std::endl;
        }
        else if (0 == strcmp("--pidfile", argv[i]) && i < argc-1) {
            s_pidFileName = new std::string(argv[++i]);
        }
        else if (0 == strcmp("--statusfile", argv[i]) && i < argc-1) {
            s_statusFileName = new std::string(argv[++i]);
        }
        else if (0 == strcmp("--recfile", argv[i]) && i < argc-1) {
            s_recordingFileName = new std::string(argv[++i]);
        }
        else if (0 == strcmp("--videobitrate", argv[i]) && i < argc-1) {
            int v = atoi(argv[++i]);
            if (v > 0 && v <= 50000)
                videoBitrate = v;
        }
        else if (0 == strcmp("--audiobitrate", argv[i]) && i < argc-1) {
            int v = atoi(argv[++i]);
            if (v > 0 && v <= 2000)
                audioBitrate = v;
        }
        else if (0 == strcmp("--keyintsecs", argv[i]) && i < argc-1) {
            int v = atoi(argv[++i]);
            if (v > 0 && v <= 999)
                keyint_secs = v;
        }
        else if (0 == strcmp("--vbr", argv[i])) {
            vbr = true;
        }
    }

    /*
    // DEBUG
    if (0) {
        s_audiopipeFileName = new std::string("/tmp/vpaudio1.fifo");
    }
    // DEBUG
    if (0) {
        s_shmemFileName = new std::string("/vpconduit1");
    }
     */

    if (s_shmemFileName) {
        g_vpObsVideo_shmemFileName = s_shmemFileName->c_str();
    }

    //std::cout << "audio pipe file: " << (s_audiopipeFileName ? *s_audiopipeFileName : "(null)") << std::endl;
    std::cout << "shmem file: " << (s_shmemFileName ? *s_shmemFileName : "(null)") << std::endl;
    std::cout << std::flush;

    /*
    if (s_cmdPipeFileName) {
        std::thread t(&commandPipeThreadFunc);
        t.detach();
    }*/

    initObsStreaming();

    resetObsVideoAndAudio();

    updateObsSettings(videoBitrate, audioBitrate, keyint_secs, vbr);

    writeStatusToFile("starting");

    startObsStreaming();

    s_streaming = true;
    s_interrupted = false;

    if (s_pidFileName) {
        std::thread t(&pidMonitorThreadFunc);
        t.detach();
    }

    // listen to interrupt signals
    signal(SIGINT, terminationSignalHandlerCb);
    signal(SIGTERM, terminationSignalHandlerCb);

    while (1) {
        std::cout << std::flush;
        usleep(10*1000);

        /*sleep(1);
        s_interrupted = true;*/

        if (s_interrupted) {
            std::cout << "Streaming stop requested." << std::endl;
            break;
        }
    }

    writeStatusToFile("stopping");

    stopObsStreaming();

    writeStatusToFile("stopped");

    std::cout << "Main thread exiting.\n";

    return 0;
}
