#pragma once

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QSet>

#include "dispatch/IHandler.h"

namespace qm {

class SessionRegistry;
class Sfu;
class StorageManager;

/// Aim: 1:1 calls and group meetings. A call is a two-person SFU room keyed by call_id; a
/// meeting is a room keyed by meeting_id and joined with a short code. Relays RtcSignal to Sfu.
/// Handles: CallInvite, CallAnswer, CallHangup, RtcSignal, MediaState,
///          MeetingCreate, MeetingJoin, MeetingLeave.
class MeetingService : public QObject, public IHandler
{
    Q_OBJECT

public:
    MeetingService(StorageManager *storage, SessionRegistry *sessions, Sfu *sfu, QObject *parent = nullptr);

    void handle(const RequestContext &context, const proto::Envelope &envelope) override;

public Q_SLOTS:
    void onUserOffline(qm::UserId user);
    void onSfuSignal(qm::UserId user, const qm::proto::RtcSignal &signal);

private:
    struct Room
    {
        QString       roomId;
        QString       code;           // empty for 1:1 calls
        QString       title;
        UserId        host = 0;
        QSet<UserId>  members;
        bool          isCall = false;
    };

    void onCallInvite(const RequestContext &context, const proto::Envelope &envelope);
    void onCallAnswer(const RequestContext &context, const proto::Envelope &envelope);
    void onCallHangup(const RequestContext &context, const proto::Envelope &envelope);
    void onRtcSignal(const RequestContext &context, const proto::Envelope &envelope);
    void onMediaState(const RequestContext &context, const proto::Envelope &envelope);
    void onMeetingCreate(const RequestContext &context, const proto::Envelope &envelope);
    void onMeetingJoin(const RequestContext &context, const proto::Envelope &envelope);
    void onMeetingLeave(const RequestContext &context, const proto::Envelope &envelope);

    void leaveRoom(const QString &roomId, UserId user);
    void notifyParticipants(const Room &room, const proto::ParticipantEvt &event);

    StorageManager       *m_storage = nullptr;
    SessionRegistry      *m_sessions = nullptr;
    Sfu                  *m_sfu = nullptr;
    QMutex                m_mutex;
    QHash<QString, Room>  m_rooms;          // key = room id
    QHash<QString, QString> m_codeToRoom;
};

} // namespace qm
