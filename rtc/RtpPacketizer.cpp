#include "RtpPacketizer.h"

namespace qm {

std::shared_ptr<rtc::MediaHandler> RtpPacketizer::createChain(RtpCodec codec,
                                                              quint32 ssrc,
                                                              const QString &cname,
                                                              int mtu)
{
    Q_UNUSED(codec)
    Q_UNUSED(ssrc)
    Q_UNUSED(cname)
    Q_UNUSED(mtu)
    return {};
}

quint32 RtpPacketizer::clockRate(RtpCodec codec)
{
    return codec == RtpCodec::H264 ? VideoClockRate : AudioClockRate;
}

int RtpPacketizer::payloadType(RtpCodec codec)
{
    return codec == RtpCodec::H264 ? H264PayloadType : OpusPayloadType;
}

} // namespace qm
