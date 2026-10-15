#include "RtpUtil.h"

namespace qm {

bool RtpUtil::parse(const QByteArray &packet, RtpInfo &info)
{
    if (packet.size() < 12)
        return false;
    const auto *p = reinterpret_cast<const uchar *>(packet.constData());
    const int version = (p[0] >> 6) & 0x3;
    if (version != 2)
        return false;
    const bool padding = p[0] & 0x20;
    const bool extension = p[0] & 0x10;
    const int csrcCount = p[0] & 0x0f;
    info.marker = p[1] & 0x80;
    info.payloadType = p[1] & 0x7f;
    info.sequence = (quint16(p[2]) << 8) | p[3];
    info.timestamp = (quint32(p[4]) << 24) | (quint32(p[5]) << 16) | (quint32(p[6]) << 8) | p[7];
    info.ssrc = (quint32(p[8]) << 24) | (quint32(p[9]) << 16) | (quint32(p[10]) << 8) | p[11];
    info.isRtcp = info.payloadType >= 64 && info.payloadType <= 95;
    int header = 12 + csrcCount * 4;
    if (extension) {
        if (packet.size() < header + 4)
            return false;
        const int extLen = ((int(p[header + 2]) << 8) | p[header + 3]) * 4;
        header += 4 + extLen;
    }
    if (padding && packet.size() > header) {
        // ignore pad length for header size
    }
    info.headerSize = header;
    return header <= packet.size();
}

void RtpUtil::rewriteSsrc(QByteArray &packet, quint32 ssrc)
{
    if (packet.size() < 12)
        return;
    auto *p = reinterpret_cast<uchar *>(packet.data());
    p[8] = uchar((ssrc >> 24) & 0xff);
    p[9] = uchar((ssrc >> 16) & 0xff);
    p[10] = uchar((ssrc >> 8) & 0xff);
    p[11] = uchar(ssrc & 0xff);
}

void RtpUtil::rewriteSequence(QByteArray &packet, quint16 sequence)
{
    if (packet.size() < 4)
        return;
    auto *p = reinterpret_cast<uchar *>(packet.data());
    p[2] = uchar((sequence >> 8) & 0xff);
    p[3] = uchar(sequence & 0xff);
}

bool RtpUtil::isH264KeyFrame(const QByteArray &packet, const RtpInfo &info)
{
    if (info.headerSize >= packet.size())
        return false;
    const auto *nal = reinterpret_cast<const uchar *>(packet.constData() + info.headerSize);
    const int type = nal[0] & 0x1f;
    if (type == 7 || type == 5) // SPS or IDR
        return true;
    if (type == 28 && packet.size() > info.headerSize + 1) { // FU-A
        const int fuType = nal[1] & 0x1f;
        const bool start = nal[1] & 0x80;
        return start && fuType == 5;
    }
    return false;
}

} // namespace qm
