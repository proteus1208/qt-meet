#pragma once

#include <QElapsedTimer>
#include <QObject>

#include <memory>

#include "common/Ids.h"
#include "media/BandwidthProfile.h"
#include "media/MediaTypes.h"

namespace qm {

class CaptureSource;
class Encoder;
class Resampler;
class Scaler;
class Sfu;

/// Aim: capture video+audio on the server and publish encoded frames into an SFU room
/// as a synthetic IPTV publisher (no client PeerConnection). Adapts geometry/fps/bitrate
/// from BandwidthProfile when viewers request limited bandwidth.
class ChannelPublisher : public QObject
{
    Q_OBJECT

public:
    ChannelPublisher(Sfu *sfu, const QString &channelId, UserId publisherId,
                     const SourceSpec &video, const SourceSpec &audio,
                     QObject *parent = nullptr);
    ~ChannelPublisher() override;

    bool start();
    void stop();
    bool isRunning() const;

    void setVideoBitrate(int bitsPerSecond);
    int  videoBitrate() const;
    BandwidthProfile profile() const;

    QString  channelId() const;
    UserId   publisherId() const;
    QString  videoLabel() const;
    QString  audioLabel() const;

Q_SIGNALS:
    void runningChanged(bool running);
    void errorOccurred(const QString &message);

private:
    void onVideo(const FrameRef &frame);
    void onAudio(const FrameRef &frame);
    void applyProfile(const BandwidthProfile &profile, bool forceReopen);
    bool openVideoEncoder(const BandwidthProfile &profile);
    bool openAudioEncoder(const BandwidthProfile &profile);
    bool shouldSendVideoFrame();
    FrameRef scaleVideoFrame(const FrameRef &frame);

    Sfu                         *m_sfu = nullptr;
    QString                      m_channelId;
    UserId                       m_publisherId = 0;
    SourceSpec                   m_video;
    SourceSpec                   m_audio;
    BandwidthProfile             m_profile;
    std::unique_ptr<CaptureSource> m_capture;
    std::unique_ptr<Encoder>       m_videoEncoder;
    std::unique_ptr<Encoder>       m_audioEncoder;
    std::unique_ptr<Scaler>        m_scaler;
    std::unique_ptr<Resampler>     m_audioResampler;
    bool                         m_running = false;
    QElapsedTimer                m_videoPaceTimer;
    qint64                       m_nextVideoSendMs = 0;
    qint64                       m_videoPts = 0;
    quint32                      m_videoTs = 0;
    quint32                      m_audioTs = 0;
};

} // namespace qm
