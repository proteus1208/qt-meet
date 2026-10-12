#pragma once

#include "MediaTypes.h"

namespace qm {

/// Aim: audio / video decoding with optional hardware frames (D3D11VA, CUDA, VAAPI, QSV)
/// and automatic software fallback.
class Decoder
{
    Q_DISABLE_COPY_MOVE(Decoder)

public:
    Decoder();
    ~Decoder();

    bool open(const AVCodecParameters *parameters, HwPolicy policy = HwPolicy::Auto);
    bool open(VideoCodec codec, HwPolicy policy = HwPolicy::Auto);   // raw RTP streams, no container
    bool open(AudioCodec codec, int sampleRate, int channels);
    void close();

    MediaResult send(const AVPacket *packet);     // nullptr = drain
    MediaResult receive(AVFrame *frame);
    void        flush();

    bool      isOpen() const;
    bool      isHardware() const;
    HwDevice  device() const;
    MediaType type() const;

private:
    bool setupHardware(HwDevice device);

    AVCodecContext *m_context = nullptr;
    AVBufferRef    *m_hwDevice = nullptr;
    HwDevice        m_device = HwDevice::None;
    MediaType       m_type = MediaType::Other;
};

} // namespace qm
