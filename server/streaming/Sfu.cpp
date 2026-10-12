#include "Sfu.h"

#include "rtc/RtcPeer.h"

namespace qm {

struct Sfu::Participant
{
    UserId                user = 0;
    RtcPeer              *peer = nullptr;
    QList<int>            publishedTracks;
    QHash<UserId, int>    preferredLayer;     // publisher -> layer
};

struct Sfu::Room
{
    QString                        roomId;
    QHash<UserId, Participant *>   participants;
};

Sfu::Sfu(const RtcConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
}

Sfu::~Sfu() = default;

void Sfu::openRoom(const QString &roomId)
{
    Q_UNUSED(roomId)
}

void Sfu::closeRoom(const QString &roomId)
{
    Q_UNUSED(roomId)
}

void Sfu::addParticipant(const QString &roomId, UserId user)
{
    Q_UNUSED(roomId)
    Q_UNUSED(user)
}

void Sfu::removeParticipant(const QString &roomId, UserId user)
{
    Q_UNUSED(roomId)
    Q_UNUSED(user)
}

void Sfu::removeEverywhere(UserId user)
{
    Q_UNUSED(user)
}

void Sfu::applySignal(UserId user, const proto::RtcSignal &signal)
{
    Q_UNUSED(user)
    Q_UNUSED(signal)
}

QList<proto::TrackInfo> Sfu::tracksOf(const QString &roomId, UserId publisher) const
{
    Q_UNUSED(roomId)
    Q_UNUSED(publisher)
    return {};
}

void Sfu::setPreferredLayer(const QString &roomId, UserId subscriber, UserId publisher, int layer)
{
    Q_UNUSED(roomId)
    Q_UNUSED(subscriber)
    Q_UNUSED(publisher)
    Q_UNUSED(layer)
}

void Sfu::onRtp(const QString &roomId, UserId publisher, int trackId, const QByteArray &packet)
{
    Q_UNUSED(roomId)
    Q_UNUSED(publisher)
    Q_UNUSED(trackId)
    Q_UNUSED(packet)
}

void Sfu::forward(Room &room, UserId publisher, int trackId, const QByteArray &packet)
{
    Q_UNUSED(room)
    Q_UNUSED(publisher)
    Q_UNUSED(trackId)
    Q_UNUSED(packet)
}

void Sfu::onKeyFrameRequest(const QString &roomId, UserId subscriber, int trackId)
{
    Q_UNUSED(roomId)
    Q_UNUSED(subscriber)
    Q_UNUSED(trackId)
}

void Sfu::renegotiate(Room &room, UserId subscriber)
{
    Q_UNUSED(room)
    Q_UNUSED(subscriber)
}

} // namespace qm
