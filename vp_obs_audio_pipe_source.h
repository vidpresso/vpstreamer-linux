//
// Created by pauli on 5/20/17.
//

#ifndef VPSTREAMER_VP_OBS_AUDIO_PIPE_SOURCE_H
#define VPSTREAMER_VP_OBS_AUDIO_PIPE_SOURCE_H


extern const char *g_vpObsAudio_pipeFileName;

#define VP_AUDIO_SOURCE_OBS_ID "vp-audio-pipe"


#ifdef __cplusplus
extern "C" {
#endif

void vp_audio_source_register();


#ifdef __cplusplus
}
#endif

#endif //VPSTREAMER_VP_OBS_AUDIO_PIPE_SOURCE_H
