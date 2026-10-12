#include "RtpDepacketizer.h"

namespace qm {

RtpDepacketizer::RtpDepacketizer(RtpCodec codec)
    : m_codec(codec)
{
}

void RtpDepacketizer::push(const QByteArray &rtpPacket)
{
    Q_UNUSED(rtpPacket)
}

bool RtpDepacketizer::takeFrame(Frame &out)
{
    Q_UNUSED(out)
    return false;
}

void RtpDepacketizer::reset()
{
    m_current.clear();
    m_gap = false;
}

} // namespace qm
