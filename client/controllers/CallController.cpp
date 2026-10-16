#include "CallController.h"

#include <QAudioDevice>
#include <QCoreApplication>
#include <QFutureWatcher>
#include <QMediaDevices>
#include <QtGlobal>

#include "common/EnvelopeFactory.h"
#include "common/Ids.h"
#include "common/Logger.h"
#include "controllers/SessionController.h"
#include "core/CallEngine.h"
#include "core/NetworkClient.h"
#include "media/BandwidthProfile.h"
#include "media/MediaSource.h"
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
    connect(m_network, &NetworkClient::envelopeReceived, this, &CallController::onEvent);
    connect(m_session, &SessionController::sessionChanged, this, [this] {
        if (!m_session->loggedIn())
            hangup();
    });
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
        hangup();
        leaveMeeting();
        leaveChannel();
    });
    if (m_engine) {
        connect(m_engine, &CallEngine::errorOccurred, this, &CallController::errorOccurred);
        connect(m_engine, &CallEngine::connectedChanged, this, [this](bool ok) {
            qCInfo(lcRtc) << "Call media connected" << ok;
        });
        connect(m_engine, &CallEngine::screenSharingChanged, this, [this](bool sharing) {
            if (m_screenSharing == sharing)
                return;
            m_screenSharing = sharing;
            Q_EMIT screenSharingChanged();
        });
    }
    refreshDevices();

    m_bandwidthTimer = new QTimer(this);
    m_bandwidthTimer->setSingleShot(true);
    m_bandwidthTimer->setInterval(250);
    connect(m_bandwidthTimer, &QTimer::timeout, this, [this] {
        applyBandwidth();
        if (m_state == State::Watching)
            postChannelBandwidth();
        else
            postMediaState();
    });
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

QString CallController::channelName() const
{
    return m_channelName;
}

QVariantList CallController::channels() const
{
    return m_channels;
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

bool CallController::speakerEnabled() const
{
    return m_speakerEnabled;
}

void CallController::setSpeakerEnabled(bool enabled)
{
    if (m_speakerEnabled == enabled)
        return;
    m_speakerEnabled = enabled;
    if (m_engine)
        m_engine->setSpeakerEnabled(enabled);
    Q_EMIT speakerEnabledChanged();
}

bool CallController::screenSharing() const
{
    return m_screenSharing;
}

void CallController::setScreenSharing(bool enabled)
{
    qCInfo(lcRtc) << "CallController::setScreenSharing" << enabled << "state" << int(m_state)
                   << "engine" << (m_engine != nullptr);

    if (enabled && !m_cameraEnabled)
        setCameraEnabled(true);

    if (m_engine) {
        m_engine->setScreenSharing(enabled);
        const bool actual = m_engine->isScreenSharing();
        if (m_screenSharing != actual) {
            m_screenSharing = actual;
            Q_EMIT screenSharingChanged();
        }
        // Keep video-source combo in sync when toggle button is used.
        for (int i = 0; i < m_videoSources.size(); ++i) {
            const bool isScreen = m_videoSources.at(i).kind == SourceKind::Screen;
            if (actual == isScreen && m_videoIndex != i) {
                m_videoIndex = i;
                Q_EMIT deviceSelectionChanged();
                break;
            }
        }
        postMediaState();
        return;
    }
    if (m_screenSharing == enabled)
        return;
    m_screenSharing = enabled;
    Q_EMIT screenSharingChanged();
}

qreal CallController::micVolume() const
{
    return m_micVolume;
}

void CallController::setMicVolume(qreal volume)
{
    volume = qBound(0.0, volume, 1.0);
    if (qFuzzyCompare(m_micVolume, volume))
        return;
    m_micVolume = volume;
    if (m_engine)
        m_engine->setMicVolume(volume);
    Q_EMIT micVolumeChanged();
}

qreal CallController::speakerVolume() const
{
    return m_speakerVolume;
}

void CallController::setSpeakerVolume(qreal volume)
{
    volume = qBound(0.0, volume, 1.0);
    if (qFuzzyCompare(m_speakerVolume, volume))
        return;
    m_speakerVolume = volume;
    if (m_engine)
        m_engine->setSpeakerVolume(volume);
    Q_EMIT speakerVolumeChanged();
}

QStringList CallController::videoDevices() const
{
    QStringList names;
    names.reserve(m_videoSources.size());
    for (const SourceSpec &spec : m_videoSources)
        names.append(spec.displayName.isEmpty() ? spec.location : spec.displayName);
    return names;
}

QStringList CallController::microphoneDevices() const
{
    QStringList names;
    names.reserve(m_microphones.size());
    for (const SourceSpec &spec : m_microphones)
        names.append(spec.displayName.isEmpty() ? spec.location : spec.displayName);
    return names;
}

QStringList CallController::speakerDevices() const
{
    return m_speakerNames;
}

int CallController::videoIndex() const
{
    return m_videoIndex;
}

void CallController::setVideoIndex(int index)
{
    if (index < 0 || index >= m_videoSources.size() || index == m_videoIndex)
        return;
    m_videoIndex = index;
    const SourceSpec &spec = m_videoSources.at(index);
    if (m_engine)
        m_engine->setCamera(spec);
    const bool sharing = spec.kind == SourceKind::Screen;
    if (m_screenSharing != sharing) {
        m_screenSharing = sharing;
        Q_EMIT screenSharingChanged();
    }
    if (sharing && !m_cameraEnabled)
        setCameraEnabled(true);
    Q_EMIT deviceSelectionChanged();
}

int CallController::microphoneIndex() const
{
    return m_microphoneIndex;
}

void CallController::setMicrophoneIndex(int index)
{
    if (index < 0 || index >= m_microphones.size() || index == m_microphoneIndex)
        return;
    m_microphoneIndex = index;
    if (m_engine)
        m_engine->setMicrophone(m_microphones.at(index));
    Q_EMIT deviceSelectionChanged();
}

int CallController::speakerIndex() const
{
    return m_speakerIndex;
}

void CallController::setSpeakerIndex(int index)
{
    if (index < 0 || index >= m_speakerIds.size() || index == m_speakerIndex)
        return;
    m_speakerIndex = index;
    if (m_engine)
        m_engine->setSpeakerId(m_speakerIds.at(index));
    Q_EMIT deviceSelectionChanged();
}

int CallController::bandwidthKbps() const
{
    return m_bandwidthKbps;
}

void CallController::setBandwidthKbps(int kbps)
{
    const int clamped = qBound(bandwidthMinKbps(), kbps, bandwidthMaxKbps());
    if (clamped == m_bandwidthKbps)
        return;
    m_bandwidthKbps = clamped;
    Q_EMIT bandwidthChanged();
    scheduleBandwidthApply();
}

int CallController::bandwidthMinKbps() const
{
    return 200;
}

int CallController::bandwidthMaxKbps() const
{
    return 4000;
}

QString CallController::bandwidthLabel() const
{
    const BandwidthProfile profile = BandwidthProfile::fromVideoBitrateKbps(m_bandwidthKbps);
    if (m_bandwidthKbps >= 1000) {
        return QStringLiteral("%1×%2 @ %3 fps · %4 Mbps")
            .arg(profile.width)
            .arg(profile.height)
            .arg(profile.fps)
            .arg(m_bandwidthKbps / 1000.0, 0, 'f', 1);
    }
    return QStringLiteral("%1×%2 @ %3 fps · %4 kbps")
        .arg(profile.width)
        .arg(profile.height)
        .arg(profile.fps)
        .arg(m_bandwidthKbps);
}

int CallController::bandwidthIndex() const
{
    return BandwidthProfile::fromVideoBitrateKbps(m_bandwidthKbps).tier;
}

void CallController::setBandwidthIndex(int index)
{
    setBandwidthKbps(bandwidthKbpsForIndex(index));
}

int CallController::bandwidthKbpsForIndex(int index)
{
    switch (index) {
    case 0:
        return 500;
    case 2:
        return 2500;
    case 3:
        return 4000;
    case 1:
    default:
        return 1200;
    }
}

void CallController::applyBandwidth()
{
    if (!m_engine)
        return;
    m_engine->setTargetVideoBitrate(m_bandwidthKbps * 1000);
}

void CallController::scheduleBandwidthApply()
{
    if (m_bandwidthTimer)
        m_bandwidthTimer->start();
    else {
        applyBandwidth();
        if (m_state == State::Watching)
            postChannelBandwidth();
        else
            postMediaState();
    }
}

void CallController::postChannelBandwidth()
{
    if (m_sessionId.isEmpty() || !m_session->loggedIn() || m_state != State::Watching)
        return;
    proto::Envelope env = EnvelopeFactory::request();
    auto *req = env.mutable_channel_bandwidth();
    req->set_channel_id(m_sessionId.toStdString());
    req->set_bandwidth_kbps(uint32_t(m_bandwidthKbps));
    m_network->post(env);
}

void CallController::refreshDevices()
{
    m_videoSources = MediaSource::enumerateCameras();
    const SourceSpec screen = MediaSource::defaultScreen();
    if (!screen.location.isEmpty()) {
        SourceSpec labeled = screen;
        if (labeled.displayName.isEmpty())
            labeled.displayName = QStringLiteral("Entire screen");
        m_videoSources.append(labeled);
    }
    // System output (WASAPI loopback / Stereo Mix) first, then real mics.
    m_microphones = MediaSource::enumerateSystemAudio();
    for (const SourceSpec &mic : MediaSource::enumerateMicrophones()) {
        bool dup = false;
        for (const SourceSpec &existing : m_microphones) {
            if (existing.location == mic.location && existing.format == mic.format) {
                dup = true;
                break;
            }
        }
        if (!dup)
            m_microphones.append(mic);
    }

    m_speakerIds.clear();
    m_speakerNames.clear();
    const auto outputs = QMediaDevices::audioOutputs();
    for (const QAudioDevice &dev : outputs) {
        m_speakerIds.append(QString::fromUtf8(dev.id()));
        m_speakerNames.append(dev.description().isEmpty() ? QStringLiteral("System output")
                                                          : dev.description());
    }

    if (m_videoIndex < 0 || m_videoIndex >= m_videoSources.size())
        m_videoIndex = 0;
    if (m_microphoneIndex < 0 || m_microphoneIndex >= m_microphones.size())
        m_microphoneIndex = 0;
    if (m_speakerIndex < 0 || m_speakerIndex >= m_speakerIds.size())
        m_speakerIndex = 0;

    applySelectedDevices();
    Q_EMIT devicesChanged();
    Q_EMIT deviceSelectionChanged();
}

void CallController::applySelectedDevices()
{
    if (!m_engine)
        return;
    if (m_videoIndex >= 0 && m_videoIndex < m_videoSources.size())
        m_engine->setCamera(m_videoSources.at(m_videoIndex));
    if (m_microphoneIndex >= 0 && m_microphoneIndex < m_microphones.size())
        m_engine->setMicrophone(m_microphones.at(m_microphoneIndex));
    if (m_speakerIndex >= 0 && m_speakerIndex < m_speakerIds.size())
        m_engine->setSpeakerId(m_speakerIds.at(m_speakerIndex));
    m_engine->setSpeakerEnabled(m_speakerEnabled);
    m_engine->setMicVolume(m_micVolume);
    m_engine->setSpeakerVolume(m_speakerVolume);
    applyBandwidth();
}

void CallController::callUser(qint64 userId, const QString &displayName, bool video)
{
    if (!m_session->loggedIn() || userId == 0 || m_state != State::Idle)
        return;

    setBandwidthKbps(m_session->defaultCallBitrateKbps());
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

    setBandwidthKbps(m_session->defaultCallBitrateKbps());
    proto::Envelope env = EnvelopeFactory::request();
    auto *ans = env.mutable_call_answer();
    ans->set_call_id(m_sessionId.toStdString());
    ans->set_accepted(true);
    m_network->post(env);

    applySelectedDevices();
    if (m_engine)
        m_engine->startSession(m_sessionId, true, m_video);
    flushPendingSignals();
    setState(State::InCall);
    postMediaState();
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
    if (m_state == State::Watching) {
        leaveChannel();
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
    if (m_screenSharing) {
        m_screenSharing = false;
        Q_EMIT screenSharingChanged();
    }
    setState(State::Idle);
    Q_EMIT callEnded(QStringLiteral("hangup"));
}

void CallController::createMeeting(const QString &title, bool video)
{
    if (!m_session->loggedIn() || m_state != State::Idle)
        return;

    setBandwidthKbps(m_session->defaultMeetingBitrateKbps());
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
    setBandwidthKbps(m_session->defaultMeetingBitrateKbps());
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
    if (m_screenSharing) {
        m_screenSharing = false;
        Q_EMIT screenSharingChanged();
    }
    setState(State::Idle);
    Q_EMIT meetingChanged();
    Q_EMIT callEnded(QStringLiteral("left"));
}

void CallController::refreshChannels()
{
    if (!m_session->loggedIn())
        return;

    proto::Envelope env = EnvelopeFactory::request();
    env.mutable_channel_list_req();

    auto *watcher = new QFutureWatcher<proto::Envelope>(this);
    connect(watcher, &QFutureWatcher<proto::Envelope>::finished, this, [this, watcher] {
        watcher->deleteLater();
        try {
            const proto::Envelope reply = watcher->result();
            if (reply.has_error()) {
                Q_EMIT errorOccurred(QString::fromStdString(reply.error().message()));
                return;
            }
            if (!reply.has_channel_list_res())
                return;
            m_channels.clear();
            for (const auto &ch : reply.channel_list_res().channels())
                upsertChannelInfo(ch);
            Q_EMIT channelsChanged();
        } catch (const std::exception &ex) {
            Q_EMIT errorOccurred(QString::fromUtf8(ex.what()));
        }
    });
    watcher->setFuture(m_network->request(env, 15000));
}

void CallController::joinChannel(const QString &channelId)
{
    if (!m_session->loggedIn() || m_state != State::Idle)
        return;
    const QString id = channelId.trimmed();
    if (id.isEmpty())
        return;

    proto::Envelope env = EnvelopeFactory::request();
    auto *req = env.mutable_channel_join();
    req->set_channel_id(id.toStdString());
    req->set_bandwidth_kbps(uint32_t(bandwidthKbps()));

    auto *watcher = new QFutureWatcher<proto::Envelope>(this);
    connect(watcher, &QFutureWatcher<proto::Envelope>::finished, this, [this, watcher] {
        watcher->deleteLater();
        try {
            const proto::Envelope reply = watcher->result();
            if (reply.has_error()) {
                Q_EMIT errorOccurred(QString::fromStdString(reply.error().message()));
                return;
            }
            if (reply.has_channel_joined())
                onChannelJoined(reply.channel_joined());
        } catch (const std::exception &ex) {
            Q_EMIT errorOccurred(QString::fromUtf8(ex.what()));
        }
    });
    watcher->setFuture(m_network->request(env, 15000));
}

void CallController::leaveChannel()
{
    if (m_state != State::Watching)
        return;

    if (!m_sessionId.isEmpty() && m_session->loggedIn()) {
        proto::Envelope env = EnvelopeFactory::request();
        env.mutable_channel_leave()->set_channel_id(m_sessionId.toStdString());
        m_network->post(env);
    }
    if (m_engine)
        m_engine->stopSession();
    m_participants->clear();
    m_pendingSignals.clear();
    m_sessionId.clear();
    m_channelName.clear();
    m_channelPublisher = 0;
    setState(State::Idle);
    Q_EMIT channelChanged();
    Q_EMIT callEnded(QStringLiteral("left-channel"));
}

void CallController::setChannelBandwidth(int bandwidthKbps)
{
    setBandwidthKbps(bandwidthKbps);
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
    case proto::Envelope::kChannelJoined:
        onChannelJoined(envelope.channel_joined());
        break;
    case proto::Envelope::kChannelEvt:
        onChannelEvent(envelope.channel_evt());
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
    m_peerName = QString::fromStdString(invite.from_display_name()).trimmed();
    if (m_peerName.isEmpty())
        m_peerName = QStringLiteral("User %1").arg(m_peer);
    m_video = invite.kind() == proto::CALL_VIDEO;

    m_participants->clear();
    proto::Participant local;
    local.mutable_user()->set_id(m_session->userId());
    local.mutable_user()->set_display_name(m_session->displayName().toStdString());
    local.set_audio_on(true);
    local.set_video_on(m_video);
    m_participants->upsert(local);
    proto::Participant remote;
    remote.mutable_user()->set_id(m_peer);
    remote.mutable_user()->set_display_name(m_peerName.toStdString());
    remote.set_audio_on(true);
    remote.set_video_on(m_video);
    m_participants->upsert(remote);

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
        setBandwidthKbps(m_session->defaultCallBitrateKbps());
        applySelectedDevices();
        if (m_engine)
            m_engine->startSession(m_sessionId, true, m_video);
        flushPendingSignals();
        setState(State::InCall);
        postMediaState();
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

void CallController::onChannelJoined(const proto::ChannelJoined &joined)
{
    if (!m_session->loggedIn())
        return;

    m_sessionId = QString::fromStdString(joined.session_id());
    if (m_sessionId.isEmpty())
        m_sessionId = QString::fromStdString(joined.channel_id());
    m_channelName = QString::fromStdString(joined.name());
    m_channelPublisher = joined.publisher_id();
    m_micEnabled = false;
    m_cameraEnabled = false;

    proto::Participant publisher;
    publisher.mutable_user()->set_id(m_channelPublisher);
    publisher.mutable_user()->set_display_name(m_channelName.toStdString());
    publisher.set_audio_on(true);
    publisher.set_video_on(true);
    for (const auto &t : joined.tracks())
        *publisher.add_tracks() = t;

    m_participants->setParticipants({publisher}, m_session->userId());

    if (m_engine) {
        m_engine->startSession(m_sessionId, false, false);
        QList<proto::TrackInfo> tracks;
        for (const auto &t : joined.tracks())
            tracks.append(t);
        m_engine->setRemoteTracks(m_channelPublisher, tracks);
        flushPendingSignals();
    }

    setState(State::Watching);
    Q_EMIT channelChanged();
}

void CallController::onChannelEvent(const proto::ChannelEvt &event)
{
    const QString id = QString::fromStdString(event.channel().id());
    switch (event.change()) {
    case proto::ChannelEvt::ADDED:
    case proto::ChannelEvt::UPDATED:
        upsertChannelInfo(event.channel());
        if (m_state == State::Watching && m_sessionId == id && !event.channel().running()) {
            leaveChannel();
            Q_EMIT errorOccurred(QStringLiteral("Channel went offline"));
        }
        break;
    case proto::ChannelEvt::REMOVED:
        removeChannelInfo(id);
        if (m_state == State::Watching && m_sessionId == id) {
            leaveChannel();
            Q_EMIT errorOccurred(QStringLiteral("Channel removed by server"));
        }
        break;
    default:
        break;
    }
    Q_EMIT channelsChanged();
}

void CallController::upsertChannelInfo(const proto::ChannelInfo &info)
{
    const QString id = QString::fromStdString(info.id());
    QVariantMap row;
    row.insert(QStringLiteral("id"), id);
    row.insert(QStringLiteral("name"), QString::fromStdString(info.name()));
    row.insert(QStringLiteral("running"), info.running());
    row.insert(QStringLiteral("hasVideo"), info.has_video());
    row.insert(QStringLiteral("hasAudio"), info.has_audio());
    row.insert(QStringLiteral("videoLabel"), QString::fromStdString(info.video_label()));
    row.insert(QStringLiteral("audioLabel"), QString::fromStdString(info.audio_label()));
    row.insert(QStringLiteral("viewers"), int(info.viewer_count()));
    row.insert(QStringLiteral("bitrateKbps"), int(info.bitrate_kbps()));

    for (int i = 0; i < m_channels.size(); ++i) {
        if (m_channels.at(i).toMap().value(QStringLiteral("id")).toString() == id) {
            m_channels[i] = row;
            return;
        }
    }
    m_channels.append(row);
}

void CallController::removeChannelInfo(const QString &channelId)
{
    for (int i = 0; i < m_channels.size(); ++i) {
        if (m_channels.at(i).toMap().value(QStringLiteral("id")).toString() == channelId) {
            m_channels.removeAt(i);
            return;
        }
    }
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
    default:
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
        return; // Watching uses ChannelBandwidth instead.
    proto::Envelope env = EnvelopeFactory::request();
    auto *state = env.mutable_media_state();
    state->set_session_id(m_sessionId.toStdString());
    state->set_audio_on(m_micEnabled);
    state->set_video_on(m_cameraEnabled);
    state->set_bandwidth_kbps(uint32_t(bandwidthKbps()));
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
    setBandwidthKbps(m_session->defaultMeetingBitrateKbps());

    QList<proto::Participant> people;
    for (const auto &p : joined.participants())
        people.append(p);
    m_participants->setParticipants(people, m_session->userId());

    if (m_engine) {
        applySelectedDevices();
        applyBandwidth();
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
    postMediaState();
    Q_EMIT meetingChanged();
}

} // namespace qm
