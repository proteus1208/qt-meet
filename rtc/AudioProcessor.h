#pragma once

#include <QtGlobal>

namespace webrtc {
class AudioProcessing;
}

namespace qm {

/// Aim: echo cancellation, noise suppression and automatic gain (webrtc-audio-processing)
/// on 10 ms int16 frames. Render = what the speakers play, capture = microphone.
class AudioProcessor
{
    Q_DISABLE_COPY_MOVE(AudioProcessor)

public:
    struct Settings
    {
        bool echoCancellation = true;
        bool noiseSuppression = true;
        bool autoGain = true;
        int  sampleRate = 48000;
        int  channels = 1;
    };

    AudioProcessor();
    ~AudioProcessor();

    bool init(const Settings &settings);
    void processRender(qint16 *samples, int frames);
    void processCapture(qint16 *samples, int frames);
    void setStreamDelayMs(int delayMs);

    bool isActive() const;
    int  frameSamples() const;               // samples per channel in one 10 ms frame

private:
    webrtc::AudioProcessing *m_apm = nullptr;
    Settings                 m_settings;
    int                      m_delayMs = 0;
};

} // namespace qm
