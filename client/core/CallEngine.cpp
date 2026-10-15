#include "CallEngine.h"

#include <cstring>

#include <QAudioDevice>
#include <QMediaDevices>
#include <QVideoFrame>

#include "common/EnvelopeFactory.h"
#include "common/Logger.h"
#include "common/ResourceMonitor.h"
#include "core/NetworkClient.h"
#include "media/AudioOutput.h"
#include "media/CaptureSource.h"
#include "media/Decoder.h"
#include "media/Encoder.h"
#include "media/FrameConverter.h"
#include "media/MediaSource.h"
#include "rtc/AudioProcessor.h"
#include "rtc/JitterBuffer.h"
#include "rtc/RtcPeer.h"
#include "rtc/RtpDepacketizer.h"
#include "rtc/RtpPacketizer.h"
#include "rtc/RtpUtil.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
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

    if (m_camera.location.isEmpty())
        m_camera = MediaSource::defaultCamera();
    if (m_microphone.location.isEmpty())
        m_microphone = MediaSource::defaultMicrophone();
    if (m_screen.location.isEmpty())
        m_screen = MediaSource::defaultScreen();

    m_peer = std::make_unique<RtcPeer>(m_config, RtcPeer::Role::Client, sessionId, this);
    connect(m_peer.get(), &RtcPeer::localSignal, this, &CallEngine::onLocalSignal);
    connect(m_peer.get(), &RtcPeer::rtpReceived, this, &CallEngine::onRemoteRtp);
    connect(m_peer.get(), &RtcPeer::stateChanged, this, [this](RtcPeer::State state) {
        Q_EMIT connectedChanged(state == RtcPeer::State::Connected);
    });

    if (audio) {
        m_audioTrack = m_peer->addTrack(RtcPeer::TrackKind::Audio, QStringLiteral("audio"), 0,
                                        RtcPeer::Direction::SendRecv);
        EncoderConfig ac;
        ac.type = MediaType::Audio;
        ac.audioCodec = AudioCodec::Opus;
        ac.sampleRate = 48000;
        ac.channels = 1;
        ac.bitrate = 32'000;
        m_audioEncoder = std::make_unique<Encoder>();
        if (!m_audioEncoder->open(ac))
            qCWarning(lcRtc) << "Opus encoder unavailable";
    }
    if (video) {
        m_videoTrack = m_peer->addTrack(RtcPeer::TrackKind::Video, QStringLiteral("video"), 0,
                                        RtcPeer::Direction::SendRecv);
        EncoderConfig vc;
        vc.type = MediaType::Video;
        vc.videoCodec = VideoCodec::H264;
        vc.width = 1280;
        vc.height = 720;
        vc.fps = 30;
        vc.bitrate = m_config.videoBitrateMax;
        vc.hwPolicy = HwPolicy::Auto;
        vc.lowLatency = true;
        m_videoEncoder = std::make_unique<Encoder>();
        if (!m_videoEncoder->open(vc))
            qCWarning(lcRtc) << "H.264 encoder unavailable";
    }

    m_capture = std::make_unique<CaptureSource>(this);
    const SourceSpec cam = activeVideoSpec();
    const SourceSpec mic = m_microphone;
    if (m_localPreview)
        m_capture->setPreviewSink(m_localPreview);
    m_capture->setVideoEnabled(video && m_cameraEnabled);
    m_capture->setAudioEnabled(audio && m_micEnabled);
    connect(m_capture.get(), &CaptureSource::videoFrame, this, &CallEngine::onCapturedVideo);
    connect(m_capture.get(), &CaptureSource::audioFrame, this, &CallEngine::onCapturedAudio);
    connect(m_capture.get(), &CaptureSource::errorOccurred, this, &CallEngine::errorOccurred);

    if (!cam.location.isEmpty() || !mic.location.isEmpty()) {
        m_capture->open(cam, mic);
        m_capture->start();
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
    m_audioOutput->open(48000, 1, outDev);
    applySpeakerMute();

    qCInfo(lcRtc) << "CallEngine session" << sessionId << "audio" << audio << "video" << video
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
    qDeleteAll(m_remoteByTrack);
    m_remoteByTrack.clear();
    m_ssrcToUser.clear();
    m_audioTrack = m_videoTrack = m_videoLowTrack = -1;
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

void CallEngine::setCamera(const SourceSpec &camera)
{
    if (camera.location.isEmpty() && camera.format.isEmpty())
        return;
    m_camera = camera;
    if (m_screenSharing)
        return;
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
    if (enabled && m_screen.location.isEmpty()) {
        m_screen = MediaSource::defaultScreen();
        if (m_screen.location.isEmpty()) {
            Q_EMIT errorOccurred(QStringLiteral("Screen capture is not available on this platform"));
            return;
        }
    }
    m_screenSharing = enabled;
    if (m_capture)
        m_capture->reopenVideo(activeVideoSpec());
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
        m_audioOutput->setVolume(1.0);
        m_audioOutput->resume();
    } else {
        m_audioOutput->setVolume(0.0);
        m_audioOutput->suspend();
    }
}

void CallEngine::onCapturedVideo(const FrameRef &frame)
{
    if (!m_cameraEnabled || !m_videoEncoder || !m_peer || m_videoTrack < 0 || !frame)
        return;
    if (m_videoEncoder->send(frame.get()) != MediaResult::Ok)
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
    if (m_audioEncoder->send(frame.get()) != MediaResult::Ok)
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
