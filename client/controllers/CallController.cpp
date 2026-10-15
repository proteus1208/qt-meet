#include "CallController.h"

#include <QFutureWatcher>

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
    postMediaState();
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
    postMediaState();
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

    if (m_state == State::InMeeting) {
        leaveMeeting();
        return;
    }

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
    if (!m_session->loggedIn() || m_state != State::Idle)
        return;

    m_video = video;
    m_cameraEnabled = video;
    m_micEnabled = true;

    proto::Envelope env = EnvelopeFactory::request();
    auto *req = env.mutable_meeting_create();
    req->set_title(title.trimmed().toStdString());
    req->set_video(video);

    auto *watcher = new QFutureWatcher<proto::Envelope>(this);
    connect(watcher, &QFutureWatcher<proto::Envelope>::finished, this, [this, watcher, video] {
        watcher->deleteLater();
        try {
            const proto::Envelope reply = watcher->result();
            if (reply.has_error()) {
                Q_EMIT errorOccurred(QString::fromStdString(reply.error().message()));
                return;
            }
            if (reply.has_meeting_created()) {
                m_sessionId = QString::fromStdString(reply.meeting_created().meeting_id());
                m_meetingCode = QString::fromStdString(reply.meeting_created().code());
                Q_EMIT meetingChanged();
            }
            // MeetingJoined arrives as a separate event and starts media.
            Q_UNUSED(video)
        } catch (const std::exception &ex) {
            Q_EMIT errorOccurred(QString::fromUtf8(ex.what()));
        }
    });
    watcher->setFuture(m_network->request(env, 15000));
}

void CallController::joinMeeting(const QString &code, bool audio, bool video)
{
    if (!m_session->loggedIn() || m_state != State::Idle)
        return;
    const QString trimmed = code.trimmed();
    if (trimmed.isEmpty()) {
        Q_EMIT errorOccurred(QStringLiteral("Enter a meeting code"));
        return;
    }

    m_video = video;
    m_cameraEnabled = video;
    m_micEnabled = audio;

    proto::Envelope env = EnvelopeFactory::request();
    auto *req = env.mutable_meeting_join();
    req->set_code(trimmed.toStdString());
    req->set_audio(audio);
    req->set_video(video);

    auto *watcher = new QFutureWatcher<proto::Envelope>(this);
    connect(watcher, &QFutureWatcher<proto::Envelope>::finished, this, [this, watcher, video] {
        watcher->deleteLater();
        try {
            const proto::Envelope reply = watcher->result();
            if (reply.has_error()) {
                Q_EMIT errorOccurred(QString::fromStdString(reply.error().message()));
                return;
            }
            if (reply.has_meeting_joined())
                enterMeeting(reply.meeting_joined(), video);
        } catch (const std::exception &ex) {
            Q_EMIT errorOccurred(QString::fromUtf8(ex.what()));
        }
    });
    watcher->setFuture(m_network->request(env, 15000));
}

void CallController::leaveMeeting()
{
    if (m_state != State::InMeeting && m_state != State::Idle)
        return;
    if (m_state == State::Idle)
        return;

    if (!m_sessionId.isEmpty() && m_session->loggedIn()) {
        proto::Envelope env = EnvelopeFactory::request();
        env.mutable_meeting_leave()->set_meeting_id(m_sessionId.toStdString());
        m_network->post(env);
    }
    if (m_engine)
        m_engine->stopSession();
    m_participants->clear();
    m_pendingSignals.clear();
    m_sessionId.clear();
    m_meetingCode.clear();
    m_meetingTitle.clear();
    setState(State::Idle);
    Q_EMIT meetingChanged();
    Q_EMIT callEnded(QStringLiteral("left"));
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
    case proto::Envelope::kMeetingCreated:
        m_sessionId = QString::fromStdString(envelope.meeting_created().meeting_id());
        m_meetingCode = QString::fromStdString(envelope.meeting_created().code());
        Q_EMIT meetingChanged();
        break;
    case proto::Envelope::kMeetingJoined:
        onMeetingJoined(envelope.meeting_joined());
        break;
    case proto::Envelope::kParticipantEvt:
        onParticipantEvent(envelope.participant_evt());
        break;
    case proto::Envelope::kError:
        Q_EMIT errorOccurred(QString::fromStdString(envelope.error().message()));
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
    enterMeeting(joined, m_video);
}

void CallController::onParticipantEvent(const proto::ParticipantEvt &event)
{
    if (!m_sessionId.isEmpty()
        && QString::fromStdString(event.meeting_id()) != m_sessionId
        && m_state == State::InMeeting) {
        return;
    }

    switch (event.change()) {
    case proto::ParticipantEvt::JOINED:
    case proto::ParticipantEvt::UPDATED:
        m_participants->upsert(event.participant());
        if (m_engine) {
            QList<proto::TrackInfo> tracks;
            for (const auto &t : event.participant().tracks())
                tracks.append(t);
            m_engine->setRemoteTracks(event.participant().user().id(), tracks);
        }
        break;
    case proto::ParticipantEvt::LEFT:
        m_participants->remove(event.participant().user().id());
        if (m_engine)
            m_engine->detachRemoteSink(event.participant().user().id());
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

void CallController::postMediaState()
{
    if (m_sessionId.isEmpty() || !m_session->loggedIn())
        return;
    if (m_state != State::InCall && m_state != State::InMeeting)
        return;
    proto::Envelope env = EnvelopeFactory::request();
    auto *state = env.mutable_media_state();
    state->set_session_id(m_sessionId.toStdString());
    state->set_audio_on(m_micEnabled);
    state->set_video_on(m_cameraEnabled);
    m_network->post(env);
}

void CallController::enterMeeting(const proto::MeetingJoined &joined, bool video)
{
    if (!m_session->loggedIn())
        return;

    const QString meetingId = QString::fromStdString(joined.meeting_id());
    if (m_state == State::InMeeting && m_sessionId == meetingId)
        return;

    m_sessionId = meetingId;
    m_meetingCode = QString::fromStdString(joined.code());
    m_meetingTitle = QString::fromStdString(joined.title());
    m_video = video;

    QList<proto::Participant> people;
    for (const auto &p : joined.participants())
        people.append(p);
    m_participants->setParticipants(people, m_session->userId());

    if (m_engine) {
        m_engine->startSession(m_sessionId, true, video && m_cameraEnabled);
        for (const auto &p : joined.participants()) {
            if (p.user().id() == m_session->userId())
                continue;
            QList<proto::TrackInfo> tracks;
            for (const auto &t : p.tracks())
                tracks.append(t);
            m_engine->setRemoteTracks(p.user().id(), tracks);
        }
        flushPendingSignals();
    }

    setState(State::InMeeting);
    Q_EMIT meetingChanged();
}

} // namespace qm
