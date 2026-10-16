#include "ChannelPublisher.h"

#include <QtGlobal>

#include "Sfu.h"
#include "common/Logger.h"
#include "media/CaptureSource.h"
#include "media/Encoder.h"
#include "media/Resampler.h"
#include "media/Scaler.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/version.h>
}

namespace qm {

ChannelPublisher::ChannelPublisher(Sfu *sfu, const QString &channelId, UserId publisherId,
                                   const SourceSpec &video, const SourceSpec &audio,
                                   QObject *parent)
    : QObject(parent)
    , m_sfu(sfu)
    , m_channelId(channelId)
    , m_publisherId(publisherId)
    , m_video(video)
    , m_audio(audio)
{
    m_profile = BandwidthProfile::fromTier(1);
    m_scaler = std::make_unique<Scaler>();
    m_audioResampler = std::make_unique<Resampler>();
}

ChannelPublisher::~ChannelPublisher()
{
    stop();
}

bool ChannelPublisher::start()
{
    if (m_running || !m_sfu)
        return m_running;

    m_sfu->openRoom(m_channelId);
    m_sfu->addLocalPublisher(m_channelId, m_publisherId);

    if (!m_video.location.isEmpty() && !openVideoEncoder(m_profile)) {
        Q_EMIT errorOccurred(QStringLiteral("Channel video encoder failed"));
    }

    if (!m_audio.location.isEmpty() && !openAudioEncoder(m_profile)) {
        Q_EMIT errorOccurred(QStringLiteral("Channel audio encoder failed"));
    }

    m_capture = std::make_unique<CaptureSource>(this);
    connect(m_capture.get(), &CaptureSource::videoFrame, this, &ChannelPublisher::onVideo);
    connect(m_capture.get(), &CaptureSource::audioFrame, this, &ChannelPublisher::onAudio);
    connect(m_capture.get(), &CaptureSource::errorOccurred, this, &ChannelPublisher::errorOccurred);

    m_capture->setOutputSize(m_profile.width, m_profile.height, m_profile.fps);
    if (!m_capture->open(m_video, m_audio)) {
        Q_EMIT errorOccurred(QStringLiteral("Cannot open channel capture devices"));
        stop();
        return false;
    }
    m_capture->setVideoEnabled(!m_video.location.isEmpty());
    m_capture->setAudioEnabled(!m_audio.location.isEmpty());
    m_capture->start();

    m_videoPaceTimer.start();
    m_nextVideoSendMs = 0;
    m_running = true;
    Q_EMIT runningChanged(true);
    qCInfo(lcRtc) << "ChannelPublisher started" << m_channelId
                   << m_profile.width << "x" << m_profile.height << "@" << m_profile.fps
                   << "vbr" << m_profile.videoBitrate
                   << "video" << m_video.displayName << "audio" << m_audio.displayName;
    return true;
}

void ChannelPublisher::stop()
{
    if (m_capture) {
        m_capture->stop();
        m_capture.reset();
    }
    m_videoEncoder.reset();
    m_audioEncoder.reset();
    if (m_scaler)
        m_scaler->reset();
    if (m_audioResampler)
        m_audioResampler->reset();
    if (m_sfu)
        m_sfu->removeLocalPublisher(m_channelId, m_publisherId);
    if (m_running) {
        m_running = false;
        Q_EMIT runningChanged(false);
    }
}

bool ChannelPublisher::isRunning() const
{
    return m_running;
}

void ChannelPublisher::setVideoBitrate(int bitsPerSecond)
{
    const int kbps = qBound(150, bitsPerSecond / 1000, 8000);
    applyProfile(BandwidthProfile::fromVideoBitrateKbps(kbps), false);
}

int ChannelPublisher::videoBitrate() const
{
    return m_profile.videoBitrate;
}

BandwidthProfile ChannelPublisher::profile() const
{
    return m_profile;
}

void ChannelPublisher::applyProfile(const BandwidthProfile &profile, bool forceReopen)
{
    const bool geometryChanged = !m_profile.sameGeometry(profile);
    const bool audioChanged = m_profile.audioBitrate != profile.audioBitrate;
    m_profile = profile;

    if (m_capture)
        m_capture->setOutputSize(m_profile.width, m_profile.height, m_profile.fps);

    if (m_videoEncoder) {
        if (forceReopen || geometryChanged) {
            if (!openVideoEncoder(m_profile))
                Q_EMIT errorOccurred(QStringLiteral("Channel video encoder reopen failed"));
            m_nextVideoSendMs = 0;
            if (m_videoPaceTimer.isValid())
                m_videoPaceTimer.restart();
        } else {
            m_videoEncoder->setBitrate(m_profile.videoBitrate);
        }
    } else if (!m_video.location.isEmpty() && m_running) {
        openVideoEncoder(m_profile);
    }

    if (m_audioEncoder) {
        if (forceReopen || audioChanged) {
            if (!openAudioEncoder(m_profile))
                Q_EMIT errorOccurred(QStringLiteral("Channel audio encoder reopen failed"));
        } else {
            m_audioEncoder->setBitrate(m_profile.audioBitrate);
        }
    }

    qCInfo(lcRtc) << "ChannelPublisher profile" << m_channelId << m_profile.tier
                   << m_profile.width << "x" << m_profile.height << "@" << m_profile.fps
                   << "v" << m_profile.videoBitrate << "a" << m_profile.audioBitrate;
}

bool ChannelPublisher::openVideoEncoder(const BandwidthProfile &profile)
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

bool ChannelPublisher::openAudioEncoder(const BandwidthProfile &profile)
{
    m_audioEncoder = std::make_unique<Encoder>();
    if (!m_audioEncoder->open(profile.audioConfig())) {
        m_audioEncoder.reset();
        return false;
    }
    if (!m_audioResampler)
        m_audioResampler = std::make_unique<Resampler>();
    m_audioResampler->configure(profile.audioSampleRate, profile.audioChannels, AV_SAMPLE_FMT_FLTP);
    return true;
}

bool ChannelPublisher::shouldSendVideoFrame()
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
    m_nextVideoSendMs = now + intervalMs;
    return true;
}

FrameRef ChannelPublisher::scaleVideoFrame(const FrameRef &frame)
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

QString ChannelPublisher::channelId() const
{
    return m_channelId;
}

UserId ChannelPublisher::publisherId() const
{
    return m_publisherId;
}

QString ChannelPublisher::videoLabel() const
{
    return m_video.displayName.isEmpty() ? m_video.location : m_video.displayName;
}

QString ChannelPublisher::audioLabel() const
{
    return m_audio.displayName.isEmpty() ? m_audio.location : m_audio.displayName;
}

void ChannelPublisher::onVideo(const FrameRef &frame)
{
    if (!m_running || !m_videoEncoder || !frame || !m_sfu)
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
        m_videoTs = quint32(packet->pts >= 0 ? packet->pts : m_videoTs + (90000 / qMax(1, m_profile.fps)));
        m_sfu->publishFrame(m_channelId, m_publisherId, true, annexb, m_videoTs);
    }
}

void ChannelPublisher::onAudio(const FrameRef &frame)
{
    if (!m_running || !m_audioEncoder || !frame || !m_sfu)
        return;

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
        const int frameSamples = m_audioEncoder->frameSize() > 0 ? m_audioEncoder->frameSize() : 960;
        m_audioTs = quint32(packet->pts >= 0 ? packet->pts : m_audioTs + quint32(frameSamples));
        m_sfu->publishFrame(m_channelId, m_publisherId, false, opus, m_audioTs);
    }
}

} // namespace qm
