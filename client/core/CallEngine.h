#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVideoSink>

#include <memory>

#include "common/Ids.h"
#include "media/MediaTypes.h"
#include "messages.pb.h"
#include "rtc/RtcConfig.h"

namespace qm {

class AudioOutput;
class AudioProcessor;
class CaptureSource;
class Encoder;
class NetworkClient;
class ResourceMonitor;
class RtcPeer;

/// Aim: the media pipeline of one call / meeting session.
///   send:    CaptureSource -> AudioProcessor (mic) -> Encoder (Opus / H.264 HW or SW) -> RtcPeer
///   receive: RtcPeer -> JitterBuffer -> RtpDepacketizer -> Decoder -> QVideoSink / audio mix
/// Adapts resolution / bitrate / HW usage from ResourceMonitor and bandwidth estimates.
class CallEngine : public QObject
{
    Q_OBJECT

public:
    CallEngine(NetworkClient *network, ResourceMonitor *monitor, const RtcConfig &config,
               QObject *parent = nullptr);
    ~CallEngine() override;

    bool startSession(const QString &sessionId, bool audio, bool video);
    void stopSession();
    bool isActive() const;

    void applySignal(const proto::RtcSignal &signal);
    void setRemoteTracks(UserId publisher, const QList<proto::TrackInfo> &tracks);

    void setMicEnabled(bool enabled);
    void setCameraEnabled(bool enabled);
    void setCamera(const SourceSpec &camera);
    void setMicrophone(const SourceSpec &microphone);

    void setLocalPreview(QVideoSink *sink);
    void attachRemoteSink(UserId user, QVideoSink *sink);
    void detachRemoteSink(UserId user);

Q_SIGNALS:
    void connectedChanged(bool connected);
    void remoteVideoActive(qm::UserId user, bool active);
    void audioLevel(qm::UserId user, float level);
    void errorOccurred(const QString &message);

private:
    struct RemoteStream;

    void onCapturedVideo(const qm::FrameRef &frame);
    void onCapturedAudio(const qm::FrameRef &frame);
    void onRemoteRtp(int trackId, const QByteArray &packet);
    void onLocalSignal(const proto::RtcSignal &signal);
    void adaptToLoad();
    void mixAndPlayAudio();

    NetworkClient                   *m_network = nullptr;
    ResourceMonitor                 *m_monitor = nullptr;
    RtcConfig                        m_config;
    QString                          m_sessionId;

    std::unique_ptr<CaptureSource>   m_capture;
    std::unique_ptr<Encoder>         m_videoEncoder;
    std::unique_ptr<Encoder>         m_audioEncoder;
    std::unique_ptr<AudioProcessor>  m_audioProcessor;
    std::unique_ptr<AudioOutput>     m_audioOutput;
    std::unique_ptr<RtcPeer>         m_peer;

    int                              m_audioTrack = -1;
    int                              m_videoTrack = -1;
    int                              m_videoLowTrack = -1;
    QHash<int, RemoteStream *>       m_remoteByTrack;
    QHash<UserId, QPointer<QVideoSink>> m_remoteSinks;
    QPointer<QVideoSink>             m_localPreview;
    bool                             m_micEnabled = true;
    bool                             m_cameraEnabled = true;
};

} // namespace qm
