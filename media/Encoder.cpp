#include "Encoder.h"

namespace qm {

Encoder::Encoder() = default;

Encoder::~Encoder()
{
    close();
}

bool Encoder::open(const EncoderConfig &config)
{
    m_config = config;
    return false;
}

void Encoder::close()
{
}

MediaResult Encoder::send(const AVFrame *frame)
{
    Q_UNUSED(frame)
    return MediaResult::Error;
}

MediaResult Encoder::receive(AVPacket *packet)
{
    Q_UNUSED(packet)
    return MediaResult::Error;
}

void Encoder::setBitrate(int bitsPerSecond)
{
    m_config.bitrate = bitsPerSecond;
}

void Encoder::requestKeyFrame()
{
    m_forceKeyFrame = true;
}

bool Encoder::fillParameters(AVCodecParameters *parameters) const
{
    Q_UNUSED(parameters)
    return false;
}

int Encoder::frameSize() const
{
    return 0;
}

bool Encoder::isOpen() const
{
    return m_context != nullptr;
}

bool Encoder::isHardware() const
{
    return false;
}

QString Encoder::encoderName() const
{
    return m_encoderName;
}

const EncoderConfig &Encoder::config() const
{
    return m_config;
}

bool Encoder::openVideo(const QString &encoderName)
{
    Q_UNUSED(encoderName)
    return false;
}

bool Encoder::openAudio()
{
    return false;
}

} // namespace qm
