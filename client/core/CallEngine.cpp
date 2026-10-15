#include "CallEngine.h"

#include <cstring>

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
#include "rtc/AudioProcessor.h"
#include "rtc/JitterBuffer.h"
#include "rtc/RtcPeer.h"
#include "rtc/RtpDepacketizer.h"
#include "rtc/RtpPacketizer.h"

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
    SourceSpec cam;
    SourceSpec mic;
#if defined(Q_OS_LINUX)
    cam.kind = SourceKind::Camera;
    cam.format = QStringLiteral("v4l2");
    cam.location = QStringLiteral("/dev/video0");
    mic.kind = SourceKind::Microphone;
    mic.format = QStringLiteral("pulse");
    mic.location = QStringLiteral("default");
#elif defined(Q_OS_WIN)
    cam.kind = SourceKind::Camera;
    cam.format = QStringLiteral("dshow");
    mic.kind = SourceKind::Microphone;
    mic.format = QStringLiteral("dshow");
#endif
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
    m_audioOutput->open(48000, 1);

    qCInfo(lcRtc) << "CallEngine session" << sessionId << "audio" << audio << "video" << video;
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
    m_audioTrack = m_videoTrack = m_videoLowTrack = -1;
    m_sessionId.clear();
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
    Q_UNUSED(publisher)
    Q_UNUSED(tracks)
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
    RemoteStream *stream = m_remoteByTrack.value(trackId);
    if (!stream) {
        stream = new RemoteStream;
        stream->jitter = std::make_unique<JitterBuffer>(90000);
        stream->depacketizer = std::make_unique<RtpDepacketizer>(RtpCodec::H264);
        stream->decoder = std::make_unique<Decoder>();
        // Best-effort: open H264 decoder for unknown tracks (audio may fail open).
        stream->decoder->open(VideoCodec::H264, HwPolicy::Auto);
        m_remoteByTrack.insert(trackId, stream);
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
            // Feed Annex-B as a packet to the decoder.
            av_new_packet(pkt.get(), frame.data.size());
            memcpy(pkt->data, frame.data.constData(), size_t(frame.data.size()));
            if (stream->decoder->send(pkt.get()) != MediaResult::Ok)
                continue;
            FrameRef decoded = makeFrame();
            while (stream->decoder->receive(decoded.get()) == MediaResult::Ok) {
                // Show on first attached remote sink (1:1).
                if (!m_remoteSinks.isEmpty()) {
                    FrameConverter converter;
                    const QVideoFrame vf = converter.toVideoFrame(decoded.get());
                    if (vf.isValid())
                        m_remoteSinks.begin().value()->setVideoFrame(vf);
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
