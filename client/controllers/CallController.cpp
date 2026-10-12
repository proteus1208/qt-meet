#include "CallController.h"

#include "controllers/SessionController.h"
#include "core/CallEngine.h"
#include "core/NetworkClient.h"
#include "models/ParticipantModel.h"

namespace qm {

CallController::CallController(NetworkClient *network, SessionController *session, CallEngine *engine,
                               ParticipantModel *participants, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_session(session)
    , m_engine(engine)
    , m_participants(participants)
{
}

CallController::State CallController::state() const
{
    return m_state;
}

QAbstractItemModel *CallController::participants() const
{
    return m_participants;
}

QString CallController::meetingCode() const
{
    return m_meetingCode;
}

QString CallController::meetingTitle() const
{
    return m_meetingTitle;
}

QString CallController::peerName() const
{
    return m_peerName;
}

bool CallController::micEnabled() const
{
    return m_micEnabled;
}

void CallController::setMicEnabled(bool enabled)
{
    if (m_micEnabled == enabled)
        return;
    m_micEnabled = enabled;
    Q_EMIT micEnabledChanged();
}

bool CallController::cameraEnabled() const
{
    return m_cameraEnabled;
}

void CallController::setCameraEnabled(bool enabled)
{
    if (m_cameraEnabled == enabled)
        return;
    m_cameraEnabled = enabled;
    Q_EMIT cameraEnabledChanged();
}

void CallController::callUser(qint64 userId, const QString &displayName, bool video)
{
    Q_UNUSED(userId)
    Q_UNUSED(displayName)
    Q_UNUSED(video)
}

void CallController::acceptCall()
{
}

void CallController::rejectCall()
{
}

void CallController::hangup()
{
}

void CallController::createMeeting(const QString &title, bool video)
{
    Q_UNUSED(title)
    Q_UNUSED(video)
}

void CallController::joinMeeting(const QString &code, bool audio, bool video)
{
    Q_UNUSED(code)
    Q_UNUSED(audio)
    Q_UNUSED(video)
}

void CallController::leaveMeeting()
{
}

void CallController::attachVideoSink(qint64 userId, QVideoSink *sink)
{
    Q_UNUSED(userId)
    Q_UNUSED(sink)
}

void CallController::detachVideoSink(qint64 userId)
{
    Q_UNUSED(userId)
}

void CallController::attachLocalPreview(QVideoSink *sink)
{
    Q_UNUSED(sink)
}

void CallController::onEvent(const proto::Envelope &envelope)
{
    Q_UNUSED(envelope)
}

void CallController::onCallInvite(const proto::CallInvite &invite)
{
    Q_UNUSED(invite)
}

void CallController::onCallAnswer(const proto::CallAnswer &answer)
{
    Q_UNUSED(answer)
}

void CallController::onCallHangup(const proto::CallHangup &hangup)
{
    Q_UNUSED(hangup)
}

void CallController::onMeetingJoined(const proto::MeetingJoined &joined)
{
    Q_UNUSED(joined)
}

void CallController::onParticipantEvent(const proto::ParticipantEvt &event)
{
    Q_UNUSED(event)
}

void CallController::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT stateChanged();
}

} // namespace qm
