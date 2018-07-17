//
// Created by pauli on 6/9/17.
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

#ifndef VPCONDUIT_VP_AUDIO_PIPE_CONSUMER_H
#define VPCONDUIT_VP_AUDIO_PIPE_CONSUMER_H

#include <string>
#include <mutex>

#define VPAUDIOSAMPLERATE 48000


typedef struct VPAudioRingBuffer VPAudioRingBuffer;


class VPAudioPipeConsumer {
public:
    // construction will start the reader thread.
    // currently there's no way to stop it, since the assumption is that VPConduit will want to read always.
    VPAudioPipeConsumer(std::string srcAudioPipePath);

    size_t readSInt16AudioData(int16_t *audioBuf, size_t audioBufSize);  // returns number of samples actually written to audioBuf

private:
    std::string m_audioPipeFile;
    std::mutex m_audioMutex;
    VPAudioRingBuffer *m_audioRingBuffer = nullptr;

    void audioPipeThreadFunc();
};

#endif //VPCONDUIT_VP_AUDIO_PIPE_CONSUMER_H
