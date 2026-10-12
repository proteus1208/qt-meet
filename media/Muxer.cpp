#include "Muxer.h"

namespace qm {

Muxer::Muxer() = default;

Muxer::~Muxer()
{
    close();
}

bool Muxer::open(const QString &url, const QString &format, const QMap<QString, QString> &options)
{
    Q_UNUSED(url)
    Q_UNUSED(format)
    Q_UNUSED(options)
    return false;
}

int Muxer::addStream(const AVCodecParameters *parameters, int timeBaseNum, int timeBaseDen)
{
    Q_UNUSED(parameters)
    Q_UNUSED(timeBaseNum)
    Q_UNUSED(timeBaseDen)
    return -1;
}

bool Muxer::writeHeader()
{
    return false;
}

bool Muxer::write(AVPacket *packet, int streamIndex)
{
    Q_UNUSED(packet)
    Q_UNUSED(streamIndex)
    return false;
}

bool Muxer::finish()
{
    return false;
}

void Muxer::close()
{
}

} // namespace qm
