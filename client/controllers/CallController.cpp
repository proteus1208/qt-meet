#include "CallController.h"

#include "common/EnvelopeFactory.h"
#include "common/Ids.h"
#include "common/Logger.h"
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
    connect(m_network, &NetworkClient::event, this, &CallController::onEvent);
    connect(m_session, &SessionController::sessionChanged, this, [this] {
        if (!m_session->loggedIn())
            hangup();
    });
    if (m_engine) {
        connect(m_engine, &CallEngine::errorOccurred, this, &CallController::errorOccurred);
        connect(m_engine, &CallEngine::connectedChanged, this, [this](bool ok) {
            qCInfo(lcRtc) << "Call media connected" << ok;
        });
    }
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

qint64 CallController::peerId() const
{
    return qint64(m_peer);
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
    if (m_engine)
        m_engine->setMicEnabled(enabled);
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
    if (m_engine)
        m_engine->setCameraEnabled(enabled);
    Q_EMIT cameraEnabledChanged();
}

void CallController::callUser(qint64 userId, const QString &displayName, bool video)
{
    if (!m_session->loggedIn() || userId == 0 || m_state != State::Idle)
        return;

    m_sessionId = Ids::uuid();
    m_peer = UserId(userId);
    m_peerName = displayName;
    m_video = video;

    proto::Envelope env = EnvelopeFactory::request();
    auto *inv = env.mutable_call_invite();
    inv->set_call_id(m_sessionId.toStdString());
    inv->set_from_user_id(m_session->userId());
    inv->set_to_user_id(m_peer);
    inv->set_kind(video ? proto::CALL_VIDEO : proto::CALL_AUDIO);
    m_network->post(env);

    m_participants->clear();
    proto::Participant local;
    local.mutable_user()->set_id(m_session->userId());
    local.mutable_user()->set_display_name(m_session->displayName().toStdString());
    local.set_audio_on(true);
    local.set_video_on(video);
    m_participants->upsert(local);
    proto::Participant remote;
    remote.mutable_user()->set_id(m_peer);
    remote.mutable_user()->set_display_name(displayName.toStdString());
    remote.set_audio_on(true);
    remote.set_video_on(video);
    m_participants->upsert(remote);

    setState(State::Outgoing);
}

void CallController::acceptCall()
{
    if (m_state != State::Incoming || m_sessionId.isEmpty())
        return;

    proto::Envelope env = EnvelopeFactory::request();
    auto *ans = env.mutable_call_answer();
    ans->set_call_id(m_sessionId.toStdString());
    ans->set_accepted(true);
    m_network->post(env);

    if (m_engine)
        m_engine->startSession(m_sessionId, true, m_video);
    flushPendingSignals();
    setState(State::InCall);
}

void CallController::rejectCall()
{
    if (m_state != State::Incoming || m_sessionId.isEmpty())
        return;
    proto::Envelope env = EnvelopeFactory::request();
    auto *ans = env.mutable_call_answer();
    ans->set_call_id(m_sessionId.toStdString());
    ans->set_accepted(false);
    m_network->post(env);
    m_participants->clear();
    m_pendingSignals.clear();
    m_sessionId.clear();
    m_peer = 0;
    setState(State::Idle);
    Q_EMIT callEnded(QStringLiteral("rejected"));
}

void CallController::hangup()
{
    if (m_state == State::Idle)
        return;

    if (!m_sessionId.isEmpty() && m_session->loggedIn()) {
        proto::Envelope env = EnvelopeFactory::request();
        auto *h = env.mutable_call_hangup();
        h->set_call_id(m_sessionId.toStdString());
        h->set_reason("hangup");
        m_network->post(env);
    }
    if (m_engine)
        m_engine->stopSession();
    m_participants->clear();
    m_pendingSignals.clear();
    m_sessionId.clear();
    m_peer = 0;
    m_peerName.clear();
    setState(State::Idle);
    Q_EMIT callEnded(QStringLiteral("hangup"));
}

void CallController::createMeeting(const QString &title, bool video)
{
    Q_UNUSED(title)
    Q_UNUSED(video)
    Q_EMIT errorOccurred(QStringLiteral("Meetings arrive in Phase 7"));
}

void CallController::joinMeeting(const QString &code, bool audio, bool video)
{
    Q_UNUSED(code)
    Q_UNUSED(audio)
    Q_UNUSED(video)
    Q_EMIT errorOccurred(QStringLiteral("Meetings arrive in Phase 7"));
}

void CallController::leaveMeeting()
{
    hangup();
}

void CallController::attachVideoSink(qint64 userId, QVideoSink *sink)
{
    if (m_engine)
        m_engine->attachRemoteSink(UserId(userId), sink);
}

void CallController::detachVideoSink(qint64 userId)
{
    if (m_engine)
        m_engine->detachRemoteSink(UserId(userId));
}

void CallController::attachLocalPreview(QVideoSink *sink)
{
    if (m_engine)
        m_engine->setLocalPreview(sink);
}

void CallController::onEvent(const proto::Envelope &envelope)
{
    switch (envelope.body_case()) {
    case proto::Envelope::kCallInvite:
        onCallInvite(envelope.call_invite());
        break;
    case proto::Envelope::kCallAnswer:
        onCallAnswer(envelope.call_answer());
        break;
    case proto::Envelope::kCallHangup:
        onCallHangup(envelope.call_hangup());
        break;
    case proto::Envelope::kRtcSignal:
        if (m_engine && m_engine->isActive())
            m_engine->applySignal(envelope.rtc_signal());
        else
            m_pendingSignals.append(envelope.rtc_signal());
        break;
    case proto::Envelope::kMeetingJoined:
        onMeetingJoined(envelope.meeting_joined());
        break;
    case proto::Envelope::kParticipantEvt:
        onParticipantEvent(envelope.participant_evt());
        break;
    default:
        break;
    }
}

void CallController::onCallInvite(const proto::CallInvite &invite)
{
    if (invite.to_user_id() != m_session->userId())
        return;
    if (m_state != State::Idle)
        return;

    m_sessionId = QString::fromStdString(invite.call_id());
    m_peer = invite.from_user_id();
    m_peerName = QStringLiteral("User %1").arg(m_peer);
    m_video = invite.kind() == proto::CALL_VIDEO;
    setState(State::Incoming);
    Q_EMIT incomingCall(m_peerName, m_video);
}

void CallController::onCallAnswer(const proto::CallAnswer &answer)
{
    if (QString::fromStdString(answer.call_id()) != m_sessionId)
        return;

    if (!answer.accepted()) {
        if (m_engine)
            m_engine->stopSession();
        m_participants->clear();
        setState(State::Idle);
        Q_EMIT callEnded(QStringLiteral("declined"));
        return;
    }

    if (m_state == State::Outgoing) {
        if (m_engine)
            m_engine->startSession(m_sessionId, true, m_video);
        flushPendingSignals();
        setState(State::InCall);
    }
}

void CallController::onCallHangup(const proto::CallHangup &hangup)
{
    if (QString::fromStdString(hangup.call_id()) != m_sessionId)
        return;
    if (m_engine)
        m_engine->stopSession();
    m_participants->clear();
    m_sessionId.clear();
    setState(State::Idle);
    Q_EMIT callEnded(QString::fromStdString(hangup.reason()));
}

void CallController::onMeetingJoined(const proto::MeetingJoined &joined)
{
    Q_UNUSED(joined)
}

void CallController::onParticipantEvent(const proto::ParticipantEvt &event)
{
    switch (event.change()) {
    case proto::ParticipantEvt::JOINED:
    case proto::ParticipantEvt::UPDATED:
        m_participants->upsert(event.participant());
        break;
    case proto::ParticipantEvt::LEFT:
        m_participants->remove(event.participant().user().id());
        break;
    }
}

void CallController::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT stateChanged();
}

void CallController::flushPendingSignals()
{
    if (!m_engine || !m_engine->isActive())
        return;
    const auto pending = m_pendingSignals;
    m_pendingSignals.clear();
    for (const proto::RtcSignal &signal : pending)
        m_engine->applySignal(signal);
}

} // namespace qm
