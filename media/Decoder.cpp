#include "Decoder.h"

namespace qm {

Decoder::Decoder() = default;

Decoder::~Decoder()
{
    close();
}

bool Decoder::open(const AVCodecParameters *parameters, HwPolicy policy)
{
    Q_UNUSED(parameters)
    Q_UNUSED(policy)
    return false;
}

bool Decoder::open(VideoCodec codec, HwPolicy policy)
{
    Q_UNUSED(codec)
    Q_UNUSED(policy)
    return false;
}

bool Decoder::open(AudioCodec codec, int sampleRate, int channels)
{
    Q_UNUSED(codec)
    Q_UNUSED(sampleRate)
    Q_UNUSED(channels)
    return false;
}

void Decoder::close()
{
}

MediaResult Decoder::send(const AVPacket *packet)
{
    Q_UNUSED(packet)
    return MediaResult::Error;
}

MediaResult Decoder::receive(AVFrame *frame)
{
    Q_UNUSED(frame)
    return MediaResult::Error;
}

void Decoder::flush()
{
}

bool Decoder::isOpen() const
{
    return m_context != nullptr;
}

bool Decoder::isHardware() const
{
    return m_device != HwDevice::None;
}

HwDevice Decoder::device() const
{
    return m_device;
}

MediaType Decoder::type() const
{
    return m_type;
}

bool Decoder::setupHardware(HwDevice device)
{
    Q_UNUSED(device)
    return false;
}

} // namespace qm
