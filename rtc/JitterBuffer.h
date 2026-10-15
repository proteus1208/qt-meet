#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>

namespace qm {

/// Aim: per-stream RTP reordering and playout timing; detects loss to produce NACK lists and
/// key-frame (PLI) requests; adapts target delay to measured network jitter.
class JitterBuffer
{
public:
    struct Stats
    {
        quint64 received = 0;
        quint64 lost = 0;
        quint64 late = 0;
        int     jitterMs = 0;
        int     delayMs = 0;
    };

    explicit JitterBuffer(quint32 clockRate, int minDelayMs = 40, int maxDelayMs = 300);

    void push(const QByteArray &rtpPacket, qint64 arrivalUs);

    /// Pops the next in-order packet whose playout time has come.
    bool pop(QByteArray &rtpPacket, qint64 nowUs);

    QList<quint16> takeNackList();
    bool           takeKeyFrameRequest();
    void           reset();
    Stats          stats() const;

private:
    void updateJitter(quint32 rtpTimestamp, qint64 arrivalUs);

    quint32                   m_clockRate;
    int                       m_minDelayMs;
    int                       m_maxDelayMs;
    QMap<quint32, QByteArray> m_packets;          // key = unwrapped sequence number
    quint32                   m_nextSequence = 0;
    bool                      m_started = false;
    bool                      m_needKeyFrame = false;
    QList<quint16>            m_missing;
    Stats                     m_stats;
};

} // namespace qm
