#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>

#include "common/Ids.h"
#include "messages.pb.h"
#include "rtc/RtcConfig.h"

namespace qm {

class RtcPeer;

/// Aim: Selective Forwarding Unit. Per room, one RtcPeer per participant (server side).
/// Incoming RTP from a publisher is forwarded (no decode) to every subscriber in the room;
/// picks the simulcast layer per subscriber and relays PLI / key-frame requests upstream.
class Sfu : public QObject
{
    Q_OBJECT

public:
    explicit Sfu(const RtcConfig &config, QObject *parent = nullptr);
    ~Sfu() override;

    void openRoom(const QString &roomId);
    void closeRoom(const QString &roomId);

    void addParticipant(const QString &roomId, UserId user);
    void removeParticipant(const QString &roomId, UserId user);
    void removeEverywhere(UserId user);

    void applySignal(UserId user, const proto::RtcSignal &signal);

    QList<proto::TrackInfo> tracksOf(const QString &roomId, UserId publisher) const;
    void setPreferredLayer(const QString &roomId, UserId subscriber, UserId publisher, int layer);

Q_SIGNALS:
    void signalForUser(qm::UserId user, const qm::proto::RtcSignal &signal);
    void tracksChanged(const QString &roomId, qm::UserId publisher);

private:
    struct Participant;
    struct Room;

    void onRtp(const QString &roomId, UserId publisher, int trackId, const QByteArray &packet);
    void forward(Room &room, UserId publisher, int trackId, const QByteArray &packet);
    void onKeyFrameRequest(const QString &roomId, UserId subscriber, int trackId);
    void renegotiate(Room &room, UserId subscriber);

    RtcConfig               m_config;
    mutable QMutex          m_mutex;
    QHash<QString, Room *>  m_rooms;
};

} // namespace qm
