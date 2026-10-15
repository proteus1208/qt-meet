#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QObject>
#include <QVideoSink>

#include "common/Ids.h"
#include "messages.pb.h"

namespace qm {

class CallEngine;
class NetworkClient;
class ParticipantModel;
class SessionController;

/// Aim: call and meeting state machine for QML: invite / accept / hang up a 1:1 call,
/// create a meeting (get code) or join one by code, toggle mic / camera, bind video tiles.
class CallController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QAbstractItemModel *participants READ participants CONSTANT)
    Q_PROPERTY(QString meetingCode READ meetingCode NOTIFY meetingChanged)
    Q_PROPERTY(QString meetingTitle READ meetingTitle NOTIFY meetingChanged)
    Q_PROPERTY(QString peerName READ peerName NOTIFY stateChanged)
    Q_PROPERTY(qint64 peerId READ peerId NOTIFY stateChanged)
    Q_PROPERTY(bool micEnabled READ micEnabled WRITE setMicEnabled NOTIFY micEnabledChanged)
    Q_PROPERTY(bool cameraEnabled READ cameraEnabled WRITE setCameraEnabled NOTIFY cameraEnabledChanged)

public:
    enum class State { Idle, Outgoing, Incoming, InCall, InMeeting };
    Q_ENUM(State)

    CallController(NetworkClient *network, SessionController *session, CallEngine *engine,
                   ParticipantModel *participants, QObject *parent = nullptr);

    State               state() const;
    QAbstractItemModel *participants() const;
    QString             meetingCode() const;
    QString             meetingTitle() const;
    QString             peerName() const;
    qint64              peerId() const;
    bool                micEnabled() const;
    void                setMicEnabled(bool enabled);
    bool                cameraEnabled() const;
    void                setCameraEnabled(bool enabled);

    Q_INVOKABLE void callUser(qint64 userId, const QString &displayName, bool video);
    Q_INVOKABLE void acceptCall();
    Q_INVOKABLE void rejectCall();
    Q_INVOKABLE void hangup();

    Q_INVOKABLE void createMeeting(const QString &title, bool video);
    Q_INVOKABLE void joinMeeting(const QString &code, bool audio, bool video);
    Q_INVOKABLE void leaveMeeting();

    Q_INVOKABLE void attachVideoSink(qint64 userId, QVideoSink *sink);
    Q_INVOKABLE void detachVideoSink(qint64 userId);
    Q_INVOKABLE void attachLocalPreview(QVideoSink *sink);

Q_SIGNALS:
    void stateChanged();
    void meetingChanged();
    void micEnabledChanged();
    void cameraEnabledChanged();
    void incomingCall(const QString &callerName, bool video);
    void callEnded(const QString &reason);
    void errorOccurred(const QString &message);

private:
    void onEvent(const proto::Envelope &envelope);
    void onCallInvite(const proto::CallInvite &invite);
    void onCallAnswer(const proto::CallAnswer &answer);
    void onCallHangup(const proto::CallHangup &hangup);
    void onMeetingJoined(const proto::MeetingJoined &joined);
    void onParticipantEvent(const proto::ParticipantEvt &event);
    void setState(State state);
    void flushPendingSignals();

    NetworkClient     *m_network = nullptr;
    SessionController *m_session = nullptr;
    CallEngine        *m_engine = nullptr;
    ParticipantModel  *m_participants = nullptr;
    State              m_state = State::Idle;
    QString            m_sessionId;           // call_id or meeting_id
    QString            m_meetingCode;
    QString            m_meetingTitle;
    QString            m_peerName;
    UserId             m_peer = 0;
    bool               m_video = true;
    bool               m_micEnabled = true;
    bool               m_cameraEnabled = true;
    QList<proto::RtcSignal> m_pendingSignals;
};

} // namespace qm
