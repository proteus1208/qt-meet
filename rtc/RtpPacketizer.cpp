#include "RtpPacketizer.h"

#include "common/Logger.h"

#if __has_include(<rtc/rtc.hpp>)
#include <rtc/rtc.hpp>
#define QM_HAVE_DATACHANNEL 1
#endif

namespace qm {

std::shared_ptr<rtc::MediaHandler> RtpPacketizer::createChain(RtpCodec codec,
                                                              quint32 ssrc,
                                                              const QString &cname,
                                                              int mtu)
{
#ifdef QM_HAVE_DATACHANNEL
    using namespace rtc;
    const int pt = payloadType(codec);
    const uint32_t clock = clockRate(codec);
    auto rtpConfig = std::make_shared<RtpPacketizationConfig>(ssrc, cname.toStdString(), pt, clock);
    rtpConfig->maxFragmentSize = mtu > 200 ? unsigned(mtu - 100) : 1000;

    if (codec == RtpCodec::H264) {
        auto packetizer = std::make_shared<H264RtpPacketizer>(NalUnit::Separator::StartSequence,
                                                              rtpConfig);
        auto sr = std::make_shared<RtcpSrReporter>(rtpConfig);
        packetizer->addToChain(sr);
        auto nack = std::make_shared<RtcpNackResponder>();
        packetizer->addToChain(nack);
        return packetizer;
    }

    auto packetizer = std::make_shared<OpusRtpPacketizer>(rtpConfig);
    auto sr = std::make_shared<RtcpSrReporter>(rtpConfig);
    packetizer->addToChain(sr);
    return packetizer;
#else
    Q_UNUSED(codec)
    Q_UNUSED(ssrc)
    Q_UNUSED(cname)
    Q_UNUSED(mtu)
    qCWarning(lcRtc) << "libdatachannel not available; RTP packetizer chain empty";
    return {};
#endif
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
