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
