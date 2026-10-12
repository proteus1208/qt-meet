#include "MediaSource.h"

namespace qm {

MediaSource::MediaSource() = default;

MediaSource::~MediaSource()
{
    close();
}

bool MediaSource::open(const SourceSpec &spec)
{
    m_spec = spec;
    return false;
}

void MediaSource::close()
{
}

bool MediaSource::isOpen() const
{
    return m_format != nullptr;
}

bool MediaSource::isLive() const
{
    return false;
}

MediaResult MediaSource::readPacket(AVPacket *packet)
{
    Q_UNUSED(packet)
    return MediaResult::Error;
}

bool MediaSource::seek(qint64 positionUs)
{
    Q_UNUSED(positionUs)
    return false;
}

void MediaSource::interrupt()
{
    m_interrupt = true;
}

QList<StreamInfo> MediaSource::streams() const
{
    return {};
}

int MediaSource::bestStream(MediaType type) const
{
    Q_UNUSED(type)
    return -1;
}

const AVCodecParameters *MediaSource::codecParameters(int streamIndex) const
{
    Q_UNUSED(streamIndex)
    return nullptr;
}

qint64 MediaSource::durationUs() const
{
    return 0;
}

QString MediaSource::lastError() const
{
    return m_error;
}

QList<SourceSpec> MediaSource::enumerateCameras()
{
    return {};
}

QList<SourceSpec> MediaSource::enumerateMicrophones()
{
    return {};
}

int MediaSource::interruptCallback(void *opaque)
{
    Q_UNUSED(opaque)
    return 0;
}

} // namespace qm
