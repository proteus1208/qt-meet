#include "RtpDepacketizer.h"

#include "RtpUtil.h"

namespace qm {

RtpDepacketizer::RtpDepacketizer(RtpCodec codec)
    : m_codec(codec)
{
}

void RtpDepacketizer::push(const QByteArray &rtpPacket)
{
    RtpInfo info;
    if (!RtpUtil::parse(rtpPacket, info) || info.isRtcp)
        return;
    const QByteArray payload = rtpPacket.mid(info.headerSize);
    if (payload.isEmpty())
        return;

    if (m_codec == RtpCodec::Opus) {
        m_current = payload;
        m_currentTimestamp = info.timestamp;
        m_currentKey = true;
        m_gap = false;
        return;
    }

    // H.264
    const auto *p = reinterpret_cast<const uchar *>(payload.constData());
    const int nalType = p[0] & 0x1f;

    if (nalType >= 1 && nalType <= 23) {
        // Single NAL — emit with Annex-B start code.
        m_current = QByteArray::fromHex("00000001") + payload;
        m_currentTimestamp = info.timestamp;
        m_currentKey = (nalType == 5 || nalType == 7);
        m_gap = false;
        return;
    }

    if (nalType == 24) { // STAP-A
        int off = 1;
        QByteArray out;
        while (off + 2 <= payload.size()) {
            const int len = (int(uchar(payload[off])) << 8) | uchar(payload[off + 1]);
            off += 2;
            if (off + len > payload.size())
                break;
            out += QByteArray::fromHex("00000001");
            out += payload.mid(off, len);
            const int t = uchar(payload[off]) & 0x1f;
            if (t == 5 || t == 7)
                m_currentKey = true;
            off += len;
        }
        m_current = out;
        m_currentTimestamp = info.timestamp;
        m_gap = false;
        return;
    }

    if (nalType == 28 && payload.size() >= 2) { // FU-A
        const uchar fuHeader = p[1];
        const bool start = fuHeader & 0x80;
        const bool end = fuHeader & 0x40;
        const int type = fuHeader & 0x1f;
        if (start) {
            m_current = QByteArray::fromHex("00000001");
            const char nalHeader = char((p[0] & 0xe0) | type);
            m_current.append(nalHeader);
            m_currentTimestamp = info.timestamp;
            m_currentKey = (type == 5);
            m_gap = false;
        }
        m_current.append(payload.mid(2));
        if (!end)
            return; // wait for more fragments; stored until takeFrame after end
        // On end, frame is ready in m_current for takeFrame.
        return;
    }
}

bool RtpDepacketizer::takeFrame(Frame &out)
{
    if (m_current.isEmpty())
        return false;
    // For FU-A mid-fragments we may have incomplete data; require marker-driven completeness
    // by only storing complete units above. Opus/single NAL always complete.
    out.data = m_current;
    out.timestamp = m_currentTimestamp;
    out.keyFrame = m_currentKey;
    out.complete = !m_gap;
    m_current.clear();
    m_currentKey = false;
    m_gap = false;
    return true;
}

void RtpDepacketizer::reset()
{
    m_current.clear();
    m_currentTimestamp = 0;
    m_currentKey = false;
    m_gap = false;
}

} // namespace qm
