#pragma once

#include <QString>

#include <memory>

namespace rtc {
class MediaHandler;
}

namespace qm {

enum class RtpCodec { H264, Opus };

/// Aim: build libdatachannel send-side handler chains per track: codec packetizer
/// (H.264 FU-A / Opus), RTCP sender reports and NACK responder.
class RtpPacketizer
{
public:
    static constexpr int H264PayloadType = 102;
    static constexpr int OpusPayloadType = 111;
    static constexpr quint32 VideoClockRate = 90000;
    static constexpr quint32 AudioClockRate = 48000;

    static std::shared_ptr<rtc::MediaHandler> createChain(RtpCodec codec,
                                                          quint32 ssrc,
                                                          const QString &cname,
                                                          int mtu);

    static quint32 clockRate(RtpCodec codec);
    static int     payloadType(RtpCodec codec);
};

} // namespace qm
