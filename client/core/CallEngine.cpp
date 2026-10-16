#include "CallEngine.h"

#include <algorithm>
#include <cstring>

#include <QAudioDevice>
#include <QMediaDevices>
#include <QtGlobal>
#include <QVideoFrame>

#include "common/EnvelopeFactory.h"
#include "common/Logger.h"
#include "common/ResourceMonitor.h"
#include "core/NetworkClient.h"
#include "media/AudioOutput.h"
#include "media/BandwidthProfile.h"
#include "media/CaptureSource.h"
#include "media/Decoder.h"
#include "media/Encoder.h"
#include "media/FrameConverter.h"
#include "media/MediaSource.h"
#include "media/Resampler.h"
#include "media/Scaler.h"
#include "rtc/AudioProcessor.h"
#include "rtc/JitterBuffer.h"
#include "rtc/RtcPeer.h"
#include "rtc/RtpDepacketizer.h"
#include "rtc/RtpPacketizer.h"
#include "rtc/RtpUtil.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/version.h>
}

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
    m_camera = MediaSource::defaultCamera();
    m_microphone = MediaSource::defaultMicrophone();
    m_screen = MediaSource::defaultScreen();
    m_profile = BandwidthProfile::fromTier(1);
    m_scaler = std::make_unique<Scaler>();
    m_audioResampler = std::make_unique<Resampler>();
}

CallEngine::~CallEngine()
{
    stopSession();
}

void CallEngine::setRtcConfig(const RtcConfig &config)
{
    m_config = config;
}

bool CallEngine::startSession(const QString &sessionId, bool audio, bool video)
{
    stopSession();
    m_sessionId = sessionId;
    m_screenSharing = false;
    m_wantAudio = audio;
    m_wantVideo = video;

    if (m_camera.location.isEmpty())
        m_camera = MediaSource::defaultCamera();
    if (m_microphone.location.isEmpty())
        m_microphone = MediaSource::defaultMicrophone();
    if (m_screen.location.isEmpty())
        m_screen = MediaSource::defaultScreen();

    const int kbps = m_targetVideoBitrate > 0 ? m_targetVideoBitrate / 1000
                                             : m_config.videoBitrateMax / 1000;
    m_profile = BandwidthProfile::fromVideoBitrateKbps(kbps);

    m_peer = std::make_unique<RtcPeer>(m_config, RtcPeer::Role::Client, sessionId, this);
    connect(m_peer.get(), &RtcPeer::localSignal, this, &CallEngine::onLocalSignal);
    connect(m_peer.get(), &RtcPeer::rtpReceived, this, &CallEngine::onRemoteRtp);
    connect(m_peer.get(), &RtcPeer::stateChanged, this, [this](RtcPeer::State state) {
        Q_EMIT connectedChanged(state == RtcPeer::State::Connected);
    });

    if (audio) {
        m_audioTrack = m_peer->addTrack(RtcPeer::TrackKind::Audio, QStringLiteral("audio"), 0,
                                        RtcPeer::Direction::SendRecv);
        if (!openAudioEncoder(m_profile))
            qCWarning(lcRtc) << "Opus encoder unavailable";
    }
    if (video) {
        m_videoTrack = m_peer->addTrack(RtcPeer::TrackKind::Video, QStringLiteral("video"), 0,
                                        RtcPeer::Direction::SendRecv);
        if (!openVideoEncoder(m_profile))
            qCWarning(lcRtc) << "H.264 encoder unavailable";
    }

    const SourceSpec cam = activeVideoSpec();
    const SourceSpec mic = m_microphone;
    if (audio || video) {
        m_capture = std::make_unique<CaptureSource>(this);
        if (m_localPreview)
            m_capture->setPreviewSink(m_localPreview);
        m_capture->setOutputSize(m_profile.width, m_profile.height, m_profile.fps);
        m_capture->setVideoEnabled(video && m_cameraEnabled);
        m_capture->setAudioEnabled(audio && m_micEnabled);
        connect(m_capture.get(), &CaptureSource::videoFrame, this, &CallEngine::onCapturedVideo);
        connect(m_capture.get(), &CaptureSource::audioFrame, this, &CallEngine::onCapturedAudio);
        connect(m_capture.get(), &CaptureSource::errorOccurred, this, &CallEngine::errorOccurred);

        if (!cam.location.isEmpty() || !mic.location.isEmpty()) {
            m_capture->open(cam, mic);
            m_capture->start();
        }
    }

    m_audioOutput = std::make_unique<AudioOutput>(this);
    QAudioDevice outDev;
    if (!m_speakerId.isEmpty()) {
        const auto outputs = QMediaDevices::audioOutputs();
        for (const QAudioDevice &dev : outputs) {
            if (dev.id() == m_speakerId.toUtf8()) {
                outDev = dev;
                break;
            }
        }
    }
    m_audioOutput->open(m_profile.audioSampleRate, m_profile.audioChannels, outDev);
    applySpeakerMute();

    m_videoPaceTimer.start();
    m_nextVideoSendMs = 0;

    qCInfo(lcRtc) << "CallEngine session" << sessionId << "audio" << audio << "video" << video
                   << "profile" << m_profile.width << "x" << m_profile.height << "@" << m_profile.fps
                   << "vbr" << m_profile.videoBitrate << "abr" << m_profile.audioBitrate
                   << "cam" << cam.displayName << "mic" << mic.displayName;
    return true;
}

void CallEngine::stopSession()
{
    if (m_capture) {
        m_capture->stop();
        m_capture.reset();
    }
    if (m_peer) {
        m_peer->close();
        m_peer.reset();
    }
    m_videoEncoder.reset();
    m_audioEncoder.reset();
    m_audioProcessor.reset();
    m_audioOutput.reset();
    if (m_scaler)
        m_scaler->reset();
    if (m_audioResampler)
        m_audioResampler->reset();
    qDeleteAll(m_remoteByTrack);
    m_remoteByTrack.clear();
    m_ssrcToUser.clear();
    m_audioTrack = m_videoTrack = m_videoLowTrack = -1;
    m_wantAudio = m_wantVideo = false;
    m_nextVideoSendMs = 0;
    m_sessionId.clear();
    if (m_screenSharing) {
        m_screenSharing = false;
        Q_EMIT screenSharingChanged(false);
    }
}

bool CallEngine::isActive() const
{
    return m_peer != nullptr;
}

void CallEngine::applySignal(const proto::RtcSignal &signal)
{
    if (m_peer)
        m_peer->applyRemoteSignal(signal);
}

void CallEngine::setRemoteTracks(UserId publisher, const QList<proto::TrackInfo> &tracks)
{
    for (const proto::TrackInfo &track : tracks) {
        if (track.ssrc() != 0)
            m_ssrcToUser.insert(quint32(track.ssrc()), publisher);
    }
}

void CallEngine::setTargetVideoBitrate(int bitsPerSecond)
{
    m_targetVideoBitrate = bitsPerSecond > 0 ? qBound(150'000, bitsPerSecond, 8'000'000) : 0;
    const int kbps = (m_targetVideoBitrate > 0 ? m_targetVideoBitrate : m_config.videoBitrateMax)
                     / 1000;
    applyBandwidthProfile(BandwidthProfile::fromVideoBitrateKbps(kbps), false);
}

int CallEngine::targetVideoBitrate() const
{
    return m_profile.videoBitrate;
}

BandwidthProfile CallEngine::bandwidthProfile() const
{
    return m_profile;
}

void CallEngine::applyBandwidthProfile(const BandwidthProfile &profile, bool forceReopen)
{
    const bool geometryChanged = !m_profile.sameGeometry(profile);
    const bool audioChanged = m_profile.audioBitrate != profile.audioBitrate
                              || m_profile.audioSampleRate != profile.audioSampleRate
                              || m_profile.audioChannels != profile.audioChannels;
    m_profile = profile;

    if (m_capture)
        m_capture->setOutputSize(m_profile.width, m_profile.height, m_profile.fps);

    if (m_wantVideo && m_videoEncoder) {
        if (forceReopen || geometryChanged) {
            openVideoEncoder(m_profile);
            m_nextVideoSendMs = 0;
            if (m_videoPaceTimer.isValid())
                m_videoPaceTimer.restart();
        } else {
            m_videoEncoder->setBitrate(m_profile.videoBitrate);
        }
    }

    if (m_wantAudio && m_audioEncoder) {
        if (forceReopen || audioChanged)
            openAudioEncoder(m_profile);
        else
            m_audioEncoder->setBitrate(m_profile.audioBitrate);
    }

    if (m_peer && m_videoTrack >= 0)
        m_peer->setBitrateHint(m_videoTrack, m_profile.videoBitrate);

    qCInfo(lcRtc) << "Bandwidth profile" << m_profile.tier << m_profile.width << "x"
                   << m_profile.height << "@" << m_profile.fps << "v" << m_profile.videoBitrate
                   << "a" << m_profile.audioBitrate;
}

bool CallEngine::openVideoEncoder(const BandwidthProfile &profile)
{
    m_videoEncoder = std::make_unique<Encoder>();
    if (!m_videoEncoder->open(profile.videoConfig())) {
        m_videoEncoder.reset();
        return false;
    }
    if (!m_scaler)
        m_scaler = std::make_unique<Scaler>();
    m_scaler->configure(profile.width, profile.height, AV_PIX_FMT_YUV420P);
    return true;
}

bool CallEngine::openAudioEncoder(const BandwidthProfile &profile)
{
    m_audioEncoder = std::make_unique<Encoder>();
    if (!m_audioEncoder->open(profile.audioConfig())) {
        m_audioEncoder.reset();
        return false;
    }
    if (!m_audioResampler)
        m_audioResampler = std::make_unique<Resampler>();
    m_audioResampler->configure(profile.audioSampleRate, profile.audioChannels,
                                AV_SAMPLE_FMT_FLTP);
    return true;
}

bool CallEngine::shouldSendVideoFrame()
{
    const int fps = qMax(1, m_profile.fps);
    const qint64 intervalMs = qMax<qint64>(1, 1000 / fps);
    if (!m_videoPaceTimer.isValid()) {
        m_videoPaceTimer.start();
        m_nextVideoSendMs = intervalMs;
        return true;
    }
    const qint64 now = m_videoPaceTimer.elapsed();
    if (now < m_nextVideoSendMs)
        return false;
    // Catch up smoothly without bursting many frames after a stall.
    m_nextVideoSendMs = now + intervalMs;
    return true;
}

FrameRef CallEngine::scaleVideoFrame(const FrameRef &frame)
{
    if (!frame || !m_scaler)
        return frame;
    if (frame->width == m_profile.width && frame->height == m_profile.height
        && frame->format == AV_PIX_FMT_YUV420P)
        return frame;

    FrameRef scaled = makeFrame();
    if (!scaled)
        return {};
    if (!m_scaler->convert(frame.get(), scaled.get()))
        return {};
    scaled->pts = frame->pts;
    return scaled;
}

void CallEngine::setMicEnabled(bool enabled)
{
    m_micEnabled = enabled;
    if (m_capture)
        m_capture->setAudioEnabled(enabled);
}

void CallEngine::setCameraEnabled(bool enabled)
{
    m_cameraEnabled = enabled;
    if (m_capture)
        m_capture->setVideoEnabled(enabled);
}

void CallEngine::setSpeakerEnabled(bool enabled)
{
    if (m_speakerEnabled == enabled)
        return;
    m_speakerEnabled = enabled;
    applySpeakerMute();
}

bool CallEngine::speakerEnabled() const
{
    return m_speakerEnabled;
}

void CallEngine::setMicVolume(qreal volume)
{
    m_micVolume = qBound(0.0, volume, 1.0);
}

qreal CallEngine::micVolume() const
{
    return m_micVolume;
}

void CallEngine::setSpeakerVolume(qreal volume)
{
    m_speakerVolume = qBound(0.0, volume, 1.0);
    applySpeakerMute();
}

qreal CallEngine::speakerVolume() const
{
    return m_speakerVolume;
}

void CallEngine::setCamera(const SourceSpec &camera)
{
    if (camera.location.isEmpty() && camera.format.isEmpty())
        return;
    if (camera.kind == SourceKind::Screen) {
        m_screen = camera;
        if (!isActive()) {
            if (m_screenSharing)
                return;
            m_screenSharing = true;
            Q_EMIT screenSharingChanged(true);
            return;
        }
        setScreenSharing(true);
        return;
    }
    m_camera = camera;
    if (m_screenSharing) {
        if (!isActive()) {
            m_screenSharing = false;
            Q_EMIT screenSharingChanged(false);
            return;
        }
        setScreenSharing(false); // restores m_camera via reopenVideo
        return;
    }
    if (m_capture)
        m_capture->reopenVideo(m_camera);
}

void CallEngine::setMicrophone(const SourceSpec &microphone)
{
    if (microphone.location.isEmpty() && microphone.format.isEmpty())
        return;
    m_microphone = microphone;
    if (m_capture)
        m_capture->reopenAudio(m_microphone);
}

void CallEngine::setSpeakerId(const QString &deviceId)
{
    if (m_speakerId == deviceId)
        return;
    m_speakerId = deviceId;
    if (!m_audioOutput)
        return;

    QAudioDevice outDev;
    if (!m_speakerId.isEmpty()) {
        const auto outputs = QMediaDevices::audioOutputs();
        for (const QAudioDevice &dev : outputs) {
            if (dev.id() == m_speakerId.toUtf8()) {
                outDev = dev;
                break;
            }
        }
    }
    m_audioOutput->open(48000, 1, outDev);
    applySpeakerMute();
}

void CallEngine::setScreenSharing(bool enabled)
{
    if (m_screenSharing == enabled)
        return;

    // Outside an active call, only remember the preference for the next startSession().
    if (!isActive()) {
        if (enabled && m_screen.location.isEmpty()) {
            m_screen = MediaSource::defaultScreen();
            if (m_screen.location.isEmpty()) {
                Q_EMIT errorOccurred(QStringLiteral("Screen capture is not available on this platform"));
                return;
            }
        }
        m_screenSharing = enabled;
        Q_EMIT screenSharingChanged(m_screenSharing);
        return;
    }

    if (enabled) {
        if (m_screen.location.isEmpty()) {
            m_screen = MediaSource::defaultScreen();
            if (m_screen.location.isEmpty()) {
                Q_EMIT errorOccurred(QStringLiteral("Screen capture is not available on this platform"));
                return;
            }
        }
        if (!ensureVideoPipeline()) {
            Q_EMIT errorOccurred(QStringLiteral("Cannot start video track for screen share"));
            return;
        }
        m_cameraEnabled = true;
        m_screenSharing = true;
        if (m_capture) {
            m_capture->setVideoEnabled(true);
            if (m_localPreview)
                m_capture->setPreviewSink(m_localPreview);
            if (!m_capture->reopenVideo(m_screen)) {
                m_screenSharing = false;
                Q_EMIT errorOccurred(QStringLiteral("Failed to open screen capture device"));
                Q_EMIT screenSharingChanged(false);
                return;
            }
        }
        qCInfo(lcRtc) << "Screen sharing started" << m_screen.format << m_screen.location;
    } else {
        m_screenSharing = false;
        if (m_capture) {
            if (m_localPreview)
                m_capture->setPreviewSink(m_localPreview);
            m_capture->reopenVideo(m_camera);
            m_capture->setVideoEnabled(m_cameraEnabled);
        }
        qCInfo(lcRtc) << "Screen sharing stopped";
    }
    Q_EMIT screenSharingChanged(m_screenSharing);
}

bool CallEngine::isScreenSharing() const
{
    return m_screenSharing;
}

SourceSpec CallEngine::camera() const
{
    return m_camera;
}

SourceSpec CallEngine::microphone() const
{
    return m_microphone;
}

QString CallEngine::speakerId() const
{
    return m_speakerId;
}

void CallEngine::setLocalPreview(QVideoSink *sink)
{
    m_localPreview = sink;
    if (m_capture)
        m_capture->setPreviewSink(sink);
}

void CallEngine::attachRemoteSink(UserId user, QVideoSink *sink)
{
    m_remoteSinks.insert(user, sink);
}

void CallEngine::detachRemoteSink(UserId user)
{
    m_remoteSinks.remove(user);
}

SourceSpec CallEngine::activeVideoSpec() const
{
    return m_screenSharing ? m_screen : m_camera;
}

void CallEngine::applySpeakerMute()
{
    if (!m_audioOutput)
        return;
    if (m_speakerEnabled) {
        m_audioOutput->setVolume(m_speakerVolume);
        m_audioOutput->resume();
    } else {
        m_audioOutput->setVolume(0.0);
        m_audioOutput->suspend();
    }
}

bool CallEngine::ensureVideoPipeline()
{
    if (!m_peer)
        return false;
    m_wantVideo = true;
    if (m_videoTrack < 0) {
        m_videoTrack = m_peer->addTrack(RtcPeer::TrackKind::Video, QStringLiteral("video"), 0,
                                        RtcPeer::Direction::SendRecv);
    }
    if (!m_videoEncoder && !openVideoEncoder(m_profile))
        return false;
    if (!m_capture) {
        m_capture = std::make_unique<CaptureSource>(this);
        if (m_localPreview)
            m_capture->setPreviewSink(m_localPreview);
        m_capture->setOutputSize(m_profile.width, m_profile.height, m_profile.fps);
        connect(m_capture.get(), &CaptureSource::videoFrame, this, &CallEngine::onCapturedVideo);
        connect(m_capture.get(), &CaptureSource::audioFrame, this, &CallEngine::onCapturedAudio);
        connect(m_capture.get(), &CaptureSource::errorOccurred, this, &CallEngine::errorOccurred);
        m_capture->open(activeVideoSpec(), m_microphone);
        m_capture->start();
    }
    return m_videoTrack >= 0 && m_videoEncoder != nullptr;
}

void CallEngine::applyPcmGain(AVFrame *frame, qreal gain)
{
    if (!frame || gain >= 0.999)
        return;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    const int channels = frame->ch_layout.nb_channels > 0 ? frame->ch_layout.nb_channels : 1;
#else
    const int channels = frame->channels > 0 ? frame->channels : 1;
#endif
    const bool planar = av_sample_fmt_is_planar(AVSampleFormat(frame->format));
    const int planes = planar ? channels : 1;
    if (gain <= 0.001) {
        for (int p = 0; p < planes; ++p) {
            if (frame->data[p] && frame->linesize[p] > 0)
                memset(frame->data[p], 0, size_t(frame->linesize[p]));
        }
        return;
    }
    if (frame->format != AV_SAMPLE_FMT_S16 && frame->format != AV_SAMPLE_FMT_S16P)
        return;
    const int samplesPerPlane = planar ? frame->nb_samples : frame->nb_samples * channels;
    for (int p = 0; p < planes; ++p) {
        auto *samples = reinterpret_cast<int16_t *>(frame->data[p]);
        if (!samples)
            continue;
        for (int i = 0; i < samplesPerPlane; ++i) {
            const int v = int(samples[i] * gain);
            samples[i] = int16_t(std::clamp(v, -32768, 32767));
        }
    }
}

void CallEngine::onCapturedVideo(const FrameRef &frame)
{
    if (!m_cameraEnabled || !m_videoEncoder || !m_peer || m_videoTrack < 0 || !frame)
        return;
    if (!shouldSendVideoFrame())
        return;

    FrameRef sendFrame = scaleVideoFrame(frame);
    if (!sendFrame)
        return;

    if (m_videoEncoder->send(sendFrame.get()) != MediaResult::Ok)
        return;
    PacketRef packet = makePacket();
    while (m_videoEncoder->receive(packet.get()) == MediaResult::Ok) {
        const QByteArray annexb(reinterpret_cast<const char *>(packet->data), int(packet->size));
        m_peer->sendFrame(m_videoTrack, annexb, quint32(packet->pts));
    }
}

void CallEngine::onCapturedAudio(const FrameRef &frame)
{
    if (!m_micEnabled || !m_audioEncoder || !m_peer || m_audioTrack < 0 || !frame)
        return;
    applyPcmGain(frame.get(), m_micVolume);

    FrameRef sendFrame = frame;
    const int srcRate = frame->sample_rate > 0 ? frame->sample_rate : m_profile.audioSampleRate;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    const int srcCh = frame->ch_layout.nb_channels > 0 ? frame->ch_layout.nb_channels
                                                      : m_profile.audioChannels;
#else
    const int srcCh = frame->channels > 0 ? frame->channels : m_profile.audioChannels;
#endif
    if (srcRate != m_profile.audioSampleRate || srcCh != m_profile.audioChannels
        || frame->format != AV_SAMPLE_FMT_FLTP) {
        if (!m_audioResampler)
            m_audioResampler = std::make_unique<Resampler>();
        m_audioResampler->configure(m_profile.audioSampleRate, m_profile.audioChannels,
                                    AV_SAMPLE_FMT_FLTP);
        FrameRef converted = makeFrame();
        if (!converted || !m_audioResampler->convert(frame.get(), converted.get()))
            return;
        converted->pts = frame->pts;
        sendFrame = converted;
    }

    if (m_audioEncoder->send(sendFrame.get()) != MediaResult::Ok)
        return;
    PacketRef packet = makePacket();
    while (m_audioEncoder->receive(packet.get()) == MediaResult::Ok) {
        const QByteArray opus(reinterpret_cast<const char *>(packet->data), int(packet->size));
        m_peer->sendFrame(m_audioTrack, opus, quint32(packet->pts));
    }
}

void CallEngine::onRemoteRtp(int trackId, const QByteArray &packet)
{
    RtpInfo info;
    UserId publisher = 0;
    if (RtpUtil::parse(packet, info)) {
        publisher = m_ssrcToUser.value(info.ssrc, 0);
        if (info.payloadType == 111 || info.payloadType == 96) {
            // 96 often used for dynamic video too; prefer kind from mapped track later.
        }
    }

    RemoteStream *stream = m_remoteByTrack.value(trackId);
    if (!stream) {
        stream = new RemoteStream;
        stream->user = publisher;
        stream->jitter = std::make_unique<JitterBuffer>(90000);
        stream->depacketizer = std::make_unique<RtpDepacketizer>(RtpCodec::H264);
        stream->decoder = std::make_unique<Decoder>();
        stream->decoder->open(VideoCodec::H264, HwPolicy::Auto);
        m_remoteByTrack.insert(trackId, stream);
    } else if (stream->user == 0 && publisher != 0) {
        stream->user = publisher;
    }

    stream->jitter->push(packet, 0);
    QByteArray ordered;
    while (stream->jitter->pop(ordered, 0)) {
        stream->depacketizer->push(ordered);
        RtpDepacketizer::Frame frame;
        while (stream->depacketizer->takeFrame(frame)) {
            PacketRef pkt = makePacket();
            if (!pkt)
                continue;
            av_new_packet(pkt.get(), frame.data.size());
            memcpy(pkt->data, frame.data.constData(), size_t(frame.data.size()));
            if (stream->decoder->send(pkt.get()) != MediaResult::Ok)
                continue;
            FrameRef decoded = makeFrame();
            while (stream->decoder->receive(decoded.get()) == MediaResult::Ok) {
                QVideoSink *sink = nullptr;
                if (stream->user != 0 && m_remoteSinks.contains(stream->user))
                    sink = m_remoteSinks.value(stream->user);
                else if (!m_remoteSinks.isEmpty())
                    sink = m_remoteSinks.begin().value();
                if (sink) {
                    FrameConverter converter;
                    const QVideoFrame vf = converter.toVideoFrame(decoded.get());
                    if (vf.isValid())
                        sink->setVideoFrame(vf);
                }
                Q_EMIT remoteVideoActive(stream->user, true);
            }
        }
    }

    if (stream->jitter->takeKeyFrameRequest() && m_peer)
        m_peer->requestKeyFrame(trackId);
}

void CallEngine::onLocalSignal(const proto::RtcSignal &signal)
{
    if (!m_network)
        return;
    proto::Envelope env = EnvelopeFactory::event();
    *env.mutable_rtc_signal() = signal;
    if (env.rtc_signal().session_id().empty())
        env.mutable_rtc_signal()->set_session_id(m_sessionId.toStdString());
    m_network->post(env);
}

void CallEngine::adaptToLoad()
{
}

void CallEngine::mixAndPlayAudio()
{
}

} // namespace qm
