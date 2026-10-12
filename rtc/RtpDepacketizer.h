#pragma once

#include <QByteArray>

#include "RtpPacketizer.h"

namespace qm {

/// Aim: reassemble ordered RTP packets (output of JitterBuffer) into complete codec frames:
/// H.264 Annex-B access units (single NAL, STAP-A, FU-A) or Opus packets.
class RtpDepacketizer
{
public:
    struct Frame
    {
        QByteArray data;
        quint32    timestamp = 0;
        bool       keyFrame = false;
        bool       complete = true;     // false when a gap was detected inside the frame
    };

    explicit RtpDepacketizer(RtpCodec codec);

    void push(const QByteArray &rtpPacket);
    bool takeFrame(Frame &out);
    void reset();

private:
    RtpCodec   m_codec;
    QByteArray m_current;
    quint32    m_currentTimestamp = 0;
    bool       m_currentKey = false;
    bool       m_gap = false;
};

} // namespace qm
