//
// Created by pauli on 5/20/17.
//

#ifndef VPSTREAMER_VP_OBS_VIDEO_SOURCE_H
#define VPSTREAMER_VP_OBS_VIDEO_SOURCE_H


#define VP_VIDEO_SOURCE_OBS_ID "vp-shmem-video-source"

#define TEST_RENDER_USING_GENPATTERN 0

extern const char *g_vpObsVideo_shmemFileName;


#ifdef __cplusplus
extern "C" {
#endif

typedef void (*VPObsVideoSourceDataCallback)(void *userData);


void vp_obs_video_source_register(VPObsVideoSourceDataCallback dataCb, void *userData);


#ifdef __cplusplus
}
#endif

#endif //VPSTREAMER_VP_OBS_VIDEO_SOURCE_H
