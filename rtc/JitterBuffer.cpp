#include "JitterBuffer.h"

#include "RtpUtil.h"

namespace qm {

JitterBuffer::JitterBuffer(quint32 clockRate, int minDelayMs, int maxDelayMs)
    : m_clockRate(clockRate > 0 ? clockRate : 90000)
    , m_minDelayMs(minDelayMs)
    , m_maxDelayMs(maxDelayMs)
{
    m_stats.delayMs = minDelayMs;
}

void JitterBuffer::push(const QByteArray &rtpPacket, qint64 arrivalUs)
{
    RtpInfo info;
    if (!RtpUtil::parse(rtpPacket, info) || info.isRtcp)
        return;

    ++m_stats.received;
    updateJitter(info.timestamp, arrivalUs);

    if (!m_started) {
        m_nextSequence = info.sequence;
        m_started = true;
    }

    // Unwrap roughly into a 32-bit key around next expected.
    quint32 key = info.sequence;
    const qint32 delta = qint16(info.sequence - quint16(m_nextSequence));
    if (delta < -1000)
        key += 0x10000u;

    if (m_packets.contains(key))
        return;
    m_packets.insert(key, rtpPacket);

    // Track gaps for NACK.
    if (delta > 1) {
        for (qint32 s = 1; s < delta && m_missing.size() < 64; ++s)
            m_missing.append(quint16(m_nextSequence + quint32(s)));
        m_needKeyFrame = true;
    }
}

bool JitterBuffer::pop(QByteArray &rtpPacket, qint64 nowUs)
{
    Q_UNUSED(nowUs)
    if (!m_started || m_packets.isEmpty())
        return false;

    auto it = m_packets.find(m_nextSequence);
    if (it == m_packets.end()) {
        // Skip if head is stuck too long (packet loss).
        if (m_packets.firstKey() > m_nextSequence + 8) {
            m_nextSequence = m_packets.firstKey();
            m_needKeyFrame = true;
            ++m_stats.lost;
            it = m_packets.find(m_nextSequence);
        } else {
            return false;
        }
    }
    if (it == m_packets.end())
        return false;

    rtpPacket = it.value();
    m_packets.erase(it);
    ++m_nextSequence;
    return true;
}

QList<quint16> JitterBuffer::takeNackList()
{
    const QList<quint16> out = m_missing;
    m_missing.clear();
    return out;
}

bool JitterBuffer::takeKeyFrameRequest()
{
    if (!m_needKeyFrame)
        return false;
    m_needKeyFrame = false;
    return true;
}

void JitterBuffer::reset()
{
    m_packets.clear();
    m_missing.clear();
    m_started = false;
    m_nextSequence = 0;
    m_needKeyFrame = false;
    m_stats = {};
    m_stats.delayMs = m_minDelayMs;
}

JitterBuffer::Stats JitterBuffer::stats() const
{
    return m_stats;
}

void JitterBuffer::updateJitter(quint32 rtpTimestamp, qint64 arrivalUs)
{
    Q_UNUSED(rtpTimestamp)
    Q_UNUSED(arrivalUs)
    // Keep delay within bounds; full RFC 3550 jitter can come later.
    m_stats.delayMs = qBound(m_minDelayMs, m_stats.delayMs, m_maxDelayMs);
}

} // namespace qm
