#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVideoSink>

#include <memory>

#include "common/Ids.h"
#include "media/BandwidthProfile.h"
#include "media/MediaTypes.h"
#include "messages.pb.h"
#include "rtc/RtcConfig.h"

struct AVFrame;

namespace qm {

class AudioOutput;
class AudioProcessor;
class CaptureSource;
class Encoder;
class NetworkClient;
class ResourceMonitor;
class Resampler;
class RtcPeer;
class Scaler;

/// Aim: the media pipeline of one call / meeting session.
///   send:    CaptureSource -> scale/pace/resample -> Encoder -> RtcPeer
///   receive: RtcPeer -> JitterBuffer -> RtpDepacketizer -> Decoder -> QVideoSink / audio mix
/// Adapts resolution / fps / bitrate / audio rate from the selected bandwidth profile.
class CallEngine : public QObject
{
    Q_OBJECT

public:
    CallEngine(NetworkClient *network, ResourceMonitor *monitor, const RtcConfig &config,
               QObject *parent = nullptr);
    ~CallEngine() override;

    void setRtcConfig(const RtcConfig &config);

    bool startSession(const QString &sessionId, bool audio, bool video);
    void stopSession();
    bool isActive() const;

    void applySignal(const proto::RtcSignal &signal);
    void setRemoteTracks(UserId publisher, const QList<proto::TrackInfo> &tracks);

    void setMicEnabled(bool enabled);
    void setCameraEnabled(bool enabled);
    void setSpeakerEnabled(bool enabled);
    bool speakerEnabled() const;

    void setTargetVideoBitrate(int bitsPerSecond);
    int  targetVideoBitrate() const;
    BandwidthProfile bandwidthProfile() const;

    void  setMicVolume(qreal volume);      // 0..1 send gain
    qreal micVolume() const;
    void  setSpeakerVolume(qreal volume);  // 0..1 playback
    qreal speakerVolume() const;

    void setCamera(const SourceSpec &camera);
    void setMicrophone(const SourceSpec &microphone);
    void setSpeakerId(const QString &deviceId);
    void setScreenSharing(bool enabled);
    bool isScreenSharing() const;

    SourceSpec camera() const;
    SourceSpec microphone() const;
    QString    speakerId() const;

    void setLocalPreview(QVideoSink *sink);
    void attachRemoteSink(UserId user, QVideoSink *sink);
    void detachRemoteSink(UserId user);

Q_SIGNALS:
    void connectedChanged(bool connected);
    void remoteVideoActive(qm::UserId user, bool active);
    void audioLevel(qm::UserId user, float level);
    void screenSharingChanged(bool sharing);
    void errorOccurred(const QString &message);

private:
    struct RemoteStream;

    void onCapturedVideo(const qm::FrameRef &frame);
    void onCapturedAudio(const qm::FrameRef &frame);
    void onRemoteRtp(int trackId, const QByteArray &packet);
    void onLocalSignal(const proto::RtcSignal &signal);
    void adaptToLoad();
    void mixAndPlayAudio();
    void applySpeakerMute();
    bool ensureVideoPipeline();
    void applyBandwidthProfile(const BandwidthProfile &profile, bool forceReopen);
    bool openVideoEncoder(const BandwidthProfile &profile);
    bool openAudioEncoder(const BandwidthProfile &profile);
    bool shouldSendVideoFrame();
    FrameRef scaleVideoFrame(const FrameRef &frame);
    SourceSpec activeVideoSpec() const;
    static void applyPcmGain(AVFrame *frame, qreal gain);

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
    std::unique_ptr<Scaler>          m_scaler;
    std::unique_ptr<Resampler>       m_audioResampler;

    SourceSpec                       m_camera;
    SourceSpec                       m_microphone;
    SourceSpec                       m_screen;
    QString                          m_speakerId;
    bool                             m_screenSharing = false;
    bool                             m_wantAudio = false;
    bool                             m_wantVideo = false;

    BandwidthProfile                 m_profile;
    int                              m_targetVideoBitrate = 0; // 0 = use RtcConfig max
    int                              m_audioTrack = -1;
    int                              m_videoTrack = -1;
    int                              m_videoLowTrack = -1;
    QElapsedTimer                    m_videoPaceTimer;
    qint64                           m_nextVideoSendMs = 0;
    QHash<int, RemoteStream *>       m_remoteByTrack;
    QHash<quint32, UserId>           m_ssrcToUser;
    QHash<UserId, QPointer<QVideoSink>> m_remoteSinks;
    QPointer<QVideoSink>             m_localPreview;
    bool                             m_micEnabled = true;
    bool                             m_cameraEnabled = true;
    bool                             m_speakerEnabled = true;
    qreal                            m_micVolume = 1.0;
    qreal                            m_speakerVolume = 1.0;
};

} // namespace qm
