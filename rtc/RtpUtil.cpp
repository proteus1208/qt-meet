#include "RtpUtil.h"

namespace qm {

bool RtpUtil::parse(const QByteArray &packet, RtpInfo &info)
{
    Q_UNUSED(packet)
    Q_UNUSED(info)
    return false;
}

void RtpUtil::rewriteSsrc(QByteArray &packet, quint32 ssrc)
{
    Q_UNUSED(packet)
    Q_UNUSED(ssrc)
}

void RtpUtil::rewriteSequence(QByteArray &packet, quint16 sequence)
{
    Q_UNUSED(packet)
    Q_UNUSED(sequence)
}

bool RtpUtil::isH264KeyFrame(const QByteArray &packet, const RtpInfo &info)
{
    Q_UNUSED(packet)
    Q_UNUSED(info)
    return false;
}

} // namespace qm
