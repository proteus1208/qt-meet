#include "JitterBuffer.h"

#include <utility>

namespace qm {

JitterBuffer::JitterBuffer(quint32 clockRate, int minDelayMs, int maxDelayMs)
    : m_clockRate(clockRate)
    , m_minDelayMs(minDelayMs)
    , m_maxDelayMs(maxDelayMs)
{
}

void JitterBuffer::push(const QByteArray &rtpPacket, qint64 arrivalUs)
{
    Q_UNUSED(rtpPacket)
    Q_UNUSED(arrivalUs)
}

bool JitterBuffer::pop(QByteArray &rtpPacket, qint64 nowUs)
{
    Q_UNUSED(rtpPacket)
    Q_UNUSED(nowUs)
    return false;
}

QList<quint16> JitterBuffer::takeNackList()
{
    return std::exchange(m_missing, {});
}

bool JitterBuffer::takeKeyFrameRequest()
{
    return std::exchange(m_needKeyFrame, false);
}

void JitterBuffer::reset()
{
    m_packets.clear();
    m_missing.clear();
    m_started = false;
    m_needKeyFrame = false;
    m_stats = {};
}

JitterBuffer::Stats JitterBuffer::stats() const
{
    return m_stats;
}

void JitterBuffer::updateJitter(quint32 rtpTimestamp, qint64 arrivalUs)
{
    Q_UNUSED(rtpTimestamp)
    Q_UNUSED(arrivalUs)
}

} // namespace qm
