#pragma once

#include <QByteArray>

namespace qm {

struct RtpInfo
{
    quint8  payloadType = 0;
    bool    marker = false;
    quint16 sequence = 0;
    quint32 timestamp = 0;
    quint32 ssrc = 0;
    int     headerSize = 0;
    bool    isRtcp = false;
};

/// Aim: zero-copy inspection / rewriting of RTP headers, used by the SFU to route and
/// rewrite packets without depacketizing them.
class RtpUtil
{
public:
    static bool parse(const QByteArray &packet, RtpInfo &info);
    static void rewriteSsrc(QByteArray &packet, quint32 ssrc);
    static void rewriteSequence(QByteArray &packet, quint16 sequence);
    static bool isH264KeyFrame(const QByteArray &packet, const RtpInfo &info);
};

} // namespace qm
