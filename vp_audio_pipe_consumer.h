//
// Created by pauli on 6/9/17.
//

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
