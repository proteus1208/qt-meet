#include "RtcPeer.h"

namespace qm {

RtcPeer::RtcPeer(const RtcConfig &config, Role role, const QString &sessionId, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_role(role)
    , m_sessionId(sessionId)
{
}

RtcPeer::~RtcPeer()
{
    close();
}

void RtcPeer::createOffer()
{
}

void RtcPeer::applyRemoteSignal(const proto::RtcSignal &signal)
{
    Q_UNUSED(signal)
}

int RtcPeer::addTrack(TrackKind kind, const QString &mid, quint32 ssrc, Direction direction)
{
    Q_UNUSED(kind)
    Q_UNUSED(mid)
    Q_UNUSED(ssrc)
    Q_UNUSED(direction)
    return -1;
}

void RtcPeer::removeTrack(int trackId)
{
    Q_UNUSED(trackId)
}

bool RtcPeer::sendFrame(int trackId, const QByteArray &frame, quint32 rtpTimestamp)
{
    Q_UNUSED(trackId)
    Q_UNUSED(frame)
    Q_UNUSED(rtpTimestamp)
    return false;
}

bool RtcPeer::sendRtp(int trackId, const QByteArray &rtpPacket)
{
    Q_UNUSED(trackId)
    Q_UNUSED(rtpPacket)
    return false;
}

void RtcPeer::requestKeyFrame(int trackId)
{
    Q_UNUSED(trackId)
}

void RtcPeer::setBitrateHint(int trackId, int bitsPerSecond)
{
    Q_UNUSED(trackId)
    Q_UNUSED(bitsPerSecond)
}

void RtcPeer::close()
{
}

RtcPeer::State RtcPeer::state() const
{
    return m_state;
}

QString RtcPeer::sessionId() const
{
    return m_sessionId;
}

void RtcPeer::setupCallbacks()
{
}

void RtcPeer::attachSendChain(int trackId, TrackKind kind, quint32 ssrc)
{
    Q_UNUSED(trackId)
    Q_UNUSED(kind)
    Q_UNUSED(ssrc)
}

} // namespace qm
