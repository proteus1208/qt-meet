#include "CallEngine.h"

#include "common/ResourceMonitor.h"
#include "core/NetworkClient.h"
#include "media/AudioOutput.h"
#include "media/CaptureSource.h"
#include "media/Decoder.h"
#include "media/Encoder.h"
#include "rtc/AudioProcessor.h"
#include "rtc/JitterBuffer.h"
#include "rtc/RtcPeer.h"
#include "rtc/RtpDepacketizer.h"

namespace qm {

struct CallEngine::RemoteStream
{
    UserId                           user = 0;
    proto::MediaKind                 kind = proto::MEDIA_AUDIO;
    std::unique_ptr<JitterBuffer>    jitter;
    std::unique_ptr<RtpDepacketizer> depacketizer;
    std::unique_ptr<Decoder>         decoder;
};

CallEngine::CallEngine(NetworkClient *network, ResourceMonitor *monitor, const RtcConfig &config,
                       QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_monitor(monitor)
    , m_config(config)
{
}

CallEngine::~CallEngine()
{
    stopSession();
}

bool CallEngine::startSession(const QString &sessionId, bool audio, bool video)
{
    Q_UNUSED(audio)
    Q_UNUSED(video)
    m_sessionId = sessionId;
    return false;
}

void CallEngine::stopSession()
{
    qDeleteAll(m_remoteByTrack);
    m_remoteByTrack.clear();
}

bool CallEngine::isActive() const
{
    return m_peer != nullptr;
}

void CallEngine::applySignal(const proto::RtcSignal &signal)
{
    Q_UNUSED(signal)
}

void CallEngine::setRemoteTracks(UserId publisher, const QList<proto::TrackInfo> &tracks)
{
    Q_UNUSED(publisher)
    Q_UNUSED(tracks)
}

void CallEngine::setMicEnabled(bool enabled)
{
    m_micEnabled = enabled;
}

void CallEngine::setCameraEnabled(bool enabled)
{
    m_cameraEnabled = enabled;
}

void CallEngine::setCamera(const SourceSpec &camera)
{
    Q_UNUSED(camera)
}

void CallEngine::setMicrophone(const SourceSpec &microphone)
{
    Q_UNUSED(microphone)
}

void CallEngine::setLocalPreview(QVideoSink *sink)
{
    m_localPreview = sink;
}

void CallEngine::attachRemoteSink(UserId user, QVideoSink *sink)
{
    m_remoteSinks.insert(user, sink);
}

void CallEngine::detachRemoteSink(UserId user)
{
    m_remoteSinks.remove(user);
}

void CallEngine::onCapturedVideo(const FrameRef &frame)
{
    Q_UNUSED(frame)
}

void CallEngine::onCapturedAudio(const FrameRef &frame)
{
    Q_UNUSED(frame)
}

void CallEngine::onRemoteRtp(int trackId, const QByteArray &packet)
{
    Q_UNUSED(trackId)
    Q_UNUSED(packet)
}

void CallEngine::onLocalSignal(const proto::RtcSignal &signal)
{
    Q_UNUSED(signal)
}

void CallEngine::adaptToLoad()
{
}

void CallEngine::mixAndPlayAudio()
{
}

} // namespace qm
