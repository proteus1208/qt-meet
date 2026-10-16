#include "Sfu.h"

#include <QRandomGenerator>

#include "common/Logger.h"
#include "rtc/RtcPeer.h"
#include "rtc/RtpUtil.h"

namespace qm {

struct Sfu::Participant
{
    UserId             user = 0;
    RtcPeer           *peer = nullptr;
    int                audioRecvTrack = -1;
    int                videoRecvTrack = -1;
    QHash<UserId, int> audioSendByPublisher; // publisher -> track id on this peer
    QHash<UserId, int> videoSendByPublisher;
    QHash<UserId, int> preferredLayer;
    quint32            audioSsrc = 0;
    quint32            videoSsrc = 0;
    bool               localPublisher = false; // IPTV server-side source
};

struct Sfu::Room
{
    QString                      roomId;
    QHash<UserId, Participant *> participants;
};

Sfu::Sfu(const RtcConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
}

Sfu::~Sfu()
{
    const QStringList ids = m_rooms.keys();
    for (const QString &id : ids)
        closeRoom(id);
}

void Sfu::openRoom(const QString &roomId)
{
    QMutexLocker locker(&m_mutex);
    if (m_rooms.contains(roomId))
        return;
    auto *room = new Room;
    room->roomId = roomId;
    m_rooms.insert(roomId, room);
    qCInfo(lcRtc) << "SFU openRoom" << roomId;
}

void Sfu::closeRoom(const QString &roomId)
{
    QMutexLocker locker(&m_mutex);
    Room *room = m_rooms.take(roomId);
    if (!room)
        return;
    for (Participant *p : room->participants) {
        if (p->peer) {
            p->peer->close();
            p->peer->deleteLater();
        }
        delete p;
    }
    delete room;
    qCInfo(lcRtc) << "SFU closeRoom" << roomId;
}

void Sfu::addParticipant(const QString &roomId, UserId user)
{
    Participant *participant = nullptr;
    Room *room = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        room = m_rooms.value(roomId);
        if (!room || room->participants.contains(user))
            return;

        participant = new Participant;
        participant->user = user;
        participant->audioSsrc = quint32(QRandomGenerator::global()->generate());
        participant->videoSsrc = quint32(QRandomGenerator::global()->generate());
        participant->peer = new RtcPeer(m_config, RtcPeer::Role::Sfu, roomId, this);

        // IPTV rooms (any local publisher): viewers only subscribe to publishers, no viewer mesh.
        bool channelRoom = false;
        for (Participant *other : room->participants) {
            if (other->localPublisher) {
                channelRoom = true;
                break;
            }
        }

        // Ingest tracks from this client (skip for IPTV viewers — receive-only).
        if (!channelRoom) {
            participant->audioRecvTrack =
                participant->peer->addTrack(RtcPeer::TrackKind::Audio, QStringLiteral("audio"),
                                            participant->audioSsrc, RtcPeer::Direction::RecvOnly);
            participant->videoRecvTrack =
                participant->peer->addTrack(RtcPeer::TrackKind::Video, QStringLiteral("video"),
                                            participant->videoSsrc, RtcPeer::Direction::RecvOnly);
        }

        for (Participant *other : room->participants) {
            if (channelRoom && !other->localPublisher)
                continue;

            const int a = participant->peer->addTrack(
                RtcPeer::TrackKind::Audio, QStringLiteral("a-%1").arg(other->user),
                other->audioSsrc, RtcPeer::Direction::SendOnly);
            const int v = participant->peer->addTrack(
                RtcPeer::TrackKind::Video, QStringLiteral("v-%1").arg(other->user),
                other->videoSsrc, RtcPeer::Direction::SendOnly);
            participant->audioSendByPublisher.insert(other->user, a);
            participant->videoSendByPublisher.insert(other->user, v);

            // Meeting mesh: existing peers also need send tracks for the newcomer.
            if (!channelRoom && other->peer && !other->localPublisher) {
                const int oa = other->peer->addTrack(
                    RtcPeer::TrackKind::Audio, QStringLiteral("a-%1").arg(user),
                    participant->audioSsrc, RtcPeer::Direction::SendOnly);
                const int ov = other->peer->addTrack(
                    RtcPeer::TrackKind::Video, QStringLiteral("v-%1").arg(user),
                    participant->videoSsrc, RtcPeer::Direction::SendOnly);
                other->audioSendByPublisher.insert(user, oa);
                other->videoSendByPublisher.insert(user, ov);
            }
        }

        room->participants.insert(user, participant);
    }

    connect(participant->peer, &RtcPeer::localSignal, this,
            [this, user](const proto::RtcSignal &signal) { Q_EMIT signalForUser(user, signal); });
    connect(participant->peer, &RtcPeer::rtpReceived, this,
            [this, roomId, user](int trackId, const QByteArray &packet) {
                onRtp(roomId, user, trackId, packet);
            });

    // Renegotiate peers that gained new send tracks.
    {
        QMutexLocker locker(&m_mutex);
        room = m_rooms.value(roomId);
        if (!room)
            return;
        for (Participant *p : room->participants) {
            if (p->user != user)
                renegotiate(*room, p->user);
        }
    }

    participant->peer->createOffer();
    Q_EMIT tracksChanged(roomId, user);
}

void Sfu::removeParticipant(const QString &roomId, UserId user)
{
    QMutexLocker locker(&m_mutex);
    Room *room = m_rooms.value(roomId);
    if (!room)
        return;
    Participant *p = room->participants.take(user);
    if (!p)
        return;
    if (p->peer) {
        p->peer->close();
        p->peer->deleteLater();
    }
    delete p;

    for (Participant *other : room->participants) {
        if (!other->peer)
            continue;
        if (const int t = other->audioSendByPublisher.take(user); t >= 0)
            other->peer->removeTrack(t);
        if (const int t = other->videoSendByPublisher.take(user); t >= 0)
            other->peer->removeTrack(t);
        renegotiate(*room, other->user);
    }

    if (room->participants.isEmpty()) {
        locker.unlock();
        closeRoom(roomId);
    }
}

void Sfu::removeEverywhere(UserId user)
{
    QStringList rooms;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_rooms.begin(); it != m_rooms.end(); ++it) {
            if (it.value()->participants.contains(user))
                rooms.append(it.key());
        }
    }
    for (const QString &roomId : rooms)
        removeParticipant(roomId, user);
}

void Sfu::applySignal(UserId user, const proto::RtcSignal &signal)
{
    QMutexLocker locker(&m_mutex);
    const QString roomId = QString::fromStdString(signal.session_id());
    Room *room = m_rooms.value(roomId);
    if (!room)
        return;
    Participant *p = room->participants.value(user);
    if (!p || !p->peer)
        return;
    p->peer->applyRemoteSignal(signal);
}

QList<proto::TrackInfo> Sfu::tracksOf(const QString &roomId, UserId publisher) const
{
    QMutexLocker locker(&m_mutex);
    Room *room = m_rooms.value(roomId);
    if (!room)
        return {};
    Participant *p = room->participants.value(publisher);
    if (!p)
        return {};
    QList<proto::TrackInfo> out;
    proto::TrackInfo audio;
    audio.set_mid("audio");
    audio.set_ssrc(p->audioSsrc);
    audio.set_kind(proto::MEDIA_AUDIO);
    out.append(audio);
    proto::TrackInfo video;
    video.set_mid("video");
    video.set_ssrc(p->videoSsrc);
    video.set_kind(proto::MEDIA_VIDEO);
    out.append(video);
    return out;
}

void Sfu::setPreferredLayer(const QString &roomId, UserId subscriber, UserId publisher, int layer)
{
    QMutexLocker locker(&m_mutex);
    Room *room = m_rooms.value(roomId);
    if (!room)
        return;
    if (Participant *p = room->participants.value(subscriber))
        p->preferredLayer.insert(publisher, layer);
}

void Sfu::onRtp(const QString &roomId, UserId publisher, int trackId, const QByteArray &packet)
{
    QMutexLocker locker(&m_mutex);
    Room *room = m_rooms.value(roomId);
    if (!room)
        return;
    Participant *pub = room->participants.value(publisher);
    if (!pub)
        return;

    // Only forward ingest tracks.
    if (trackId != pub->audioRecvTrack && trackId != pub->videoRecvTrack)
        return;

    forward(*room, publisher, trackId, packet);
}

void Sfu::forward(Room &room, UserId publisher, int trackId, const QByteArray &packet)
{
    Participant *pub = room.participants.value(publisher);
    if (!pub)
        return;
    const bool isVideo = (trackId == pub->videoRecvTrack);

    for (Participant *sub : room.participants) {
        if (sub->user == publisher)
            continue;
        const int outTrack = isVideo ? sub->videoSendByPublisher.value(publisher, -1)
                                     : sub->audioSendByPublisher.value(publisher, -1);
        if (outTrack < 0 || !sub->peer)
            continue;
        QByteArray copy = packet;
        RtpUtil::rewriteSsrc(copy, isVideo ? pub->videoSsrc : pub->audioSsrc);
        sub->peer->sendRtp(outTrack, copy);
    }
}

void Sfu::onKeyFrameRequest(const QString &roomId, UserId subscriber, int trackId)
{
    Q_UNUSED(subscriber)
    QMutexLocker locker(&m_mutex);
    Room *room = m_rooms.value(roomId);
    if (!room)
        return;
    for (Participant *p : room->participants) {
        if (p->videoRecvTrack == trackId && p->peer) {
            p->peer->requestKeyFrame(trackId);
            return;
        }
    }
}

void Sfu::renegotiate(Room &room, UserId subscriber)
{
    Participant *p = room.participants.value(subscriber);
    if (p && p->peer)
        p->peer->createOffer();
}

void Sfu::addLocalPublisher(const QString &roomId, UserId publisherId)
{
    openRoom(roomId);
    QList<UserId> renegotiateUsers;
    {
        QMutexLocker locker(&m_mutex);
        Room *room = m_rooms.value(roomId);
        if (!room || room->participants.contains(publisherId))
            return;

        auto *publisher = new Participant;
        publisher->user = publisherId;
        publisher->localPublisher = true;
        publisher->audioSsrc = quint32(QRandomGenerator::global()->generate());
        publisher->videoSsrc = quint32(QRandomGenerator::global()->generate());
        room->participants.insert(publisherId, publisher);

        for (Participant *viewer : room->participants) {
            if (viewer->user == publisherId || !viewer->peer)
                continue;
            const int a = viewer->peer->addTrack(
                RtcPeer::TrackKind::Audio, QStringLiteral("a-%1").arg(publisherId),
                publisher->audioSsrc, RtcPeer::Direction::SendOnly);
            const int v = viewer->peer->addTrack(
                RtcPeer::TrackKind::Video, QStringLiteral("v-%1").arg(publisherId),
                publisher->videoSsrc, RtcPeer::Direction::SendOnly);
            viewer->audioSendByPublisher.insert(publisherId, a);
            viewer->videoSendByPublisher.insert(publisherId, v);
            renegotiateUsers.append(viewer->user);
        }
    }
    for (UserId user : renegotiateUsers) {
        QMutexLocker locker(&m_mutex);
        if (Room *room = m_rooms.value(roomId))
            renegotiate(*room, user);
    }
    Q_EMIT tracksChanged(roomId, publisherId);
    qCInfo(lcRtc) << "SFU local publisher" << publisherId << "in" << roomId;
}

void Sfu::removeLocalPublisher(const QString &roomId, UserId publisherId)
{
    removeParticipant(roomId, publisherId);
}

bool Sfu::publishFrame(const QString &roomId, UserId publisherId, bool video,
                       const QByteArray &encodedFrame, quint32 rtpTimestamp)
{
    if (encodedFrame.isEmpty())
        return false;
    QMutexLocker locker(&m_mutex);
    Room *room = m_rooms.value(roomId);
    if (!room)
        return false;
    Participant *pub = room->participants.value(publisherId);
    if (!pub || !pub->localPublisher)
        return false;

    bool any = false;
    for (Participant *sub : room->participants) {
        if (sub->user == publisherId || !sub->peer)
            continue;
        const int outTrack = video ? sub->videoSendByPublisher.value(publisherId, -1)
                                   : sub->audioSendByPublisher.value(publisherId, -1);
        if (outTrack < 0)
            continue;
        if (sub->peer->sendFrame(outTrack, encodedFrame, rtpTimestamp))
            any = true;
    }
    return any;
}

} // namespace qm
