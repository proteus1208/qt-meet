#pragma once

#include "MediaTypes.h"

namespace qm {

/// Aim: video (H.264, HW or software) and audio (Opus) encoding tuned for low latency,
/// with runtime bitrate change and on-demand key frames.
class Encoder
{
    Q_DISABLE_COPY_MOVE(Encoder)

public:
    Encoder();
    ~Encoder();

    bool open(const EncoderConfig &config);
    void close();

    MediaResult send(const AVFrame *frame);       // nullptr = drain
    MediaResult receive(AVPacket *packet);

    void setBitrate(int bitsPerSecond);
    void requestKeyFrame();

    bool fillParameters(AVCodecParameters *parameters) const;
    int  frameSize() const;                       // audio samples per frame (Opus: 960 @ 48 kHz)

    bool                 isOpen() const;
    bool                 isHardware() const;
    QString              encoderName() const;
    const EncoderConfig &config() const;

private:
    bool openVideo(const QString &encoderName);
    bool openAudio();

    AVCodecContext *m_context = nullptr;
    AVBufferRef    *m_hwFrames = nullptr;
    EncoderConfig   m_config;
    QString         m_encoderName;
    bool            m_forceKeyFrame = false;
};

} // namespace qm
