#include "MediaPlayer.h"

#include <memory>

#include <QMetaObject>
#include <QMutex>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>

#include "AudioOutput.h"
#include "Decoder.h"
#include "FrameConverter.h"
#include "MediaClock.h"
#include "MediaSource.h"
#include "Resampler.h"
#include "common/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
#include <libavutil/time.h>
}

namespace qm {

namespace {

SourceSpec specFromUrl(const QUrl &url)
{
    SourceSpec spec;
    const QString s = url.toString();
    if (url.isLocalFile()) {
        spec.kind = SourceKind::File;
        spec.location = url.toLocalFile();
    } else if (s.startsWith(QStringLiteral("rtsp://"), Qt::CaseInsensitive)) {
        spec.kind = SourceKind::Rtsp;
        spec.location = s;
    } else {
        spec.kind = SourceKind::Url;
        spec.location = s;
    }
    return spec;
}

qint64 ptsToUs(qint64 pts, AVRational tb)
{
    if (pts == AV_NOPTS_VALUE)
        return 0;
    return av_rescale_q(pts, tb, {1, AV_TIME_BASE});
}

} // namespace

class PlaybackEngine : public QObject
{
    Q_OBJECT

public:
    explicit PlaybackEngine(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_timer = new QTimer(this);
        m_timer->setInterval(5);
        connect(m_timer, &QTimer::timeout, this, &PlaybackEngine::pump);
    }

public Q_SLOTS:
    void open(const QUrl &url, qm::HwPolicy policy)
    {
        stopInternal();
        m_hwPolicy = policy;
        m_sourceSpec = specFromUrl(url);
        if (!m_demux.open(m_sourceSpec)) {
            Q_EMIT error(m_demux.lastError().isEmpty() ? QStringLiteral("Cannot open media")
                                                       : m_demux.lastError());
            return;
        }

        m_videoStream = m_demux.bestStream(MediaType::Video);
        m_audioStream = m_demux.bestStream(MediaType::Audio);

        if (m_videoStream >= 0) {
            // File/HTTP player pushes CPU frames into QVideoFrame. D3D11VA/HEVC often
            // yields black VideoOutput on Windows even when decode succeeds — prefer software.
            HwPolicy policy = m_hwPolicy;
            if (policy == HwPolicy::Auto)
                policy = HwPolicy::SoftwareOnly;
            if (!m_videoDecoder.open(m_demux.codecParameters(m_videoStream), policy)) {
                Q_EMIT error(QStringLiteral("Cannot open video decoder"));
                return;
            }
            Q_EMIT hardwareChanged(m_videoDecoder.isHardware());
        }
        if (m_audioStream >= 0) {
            if (!m_audioDecoder.open(m_demux.codecParameters(m_audioStream), HwPolicy::SoftwareOnly)) {
                qCWarning(lcMedia) << "Audio decoder open failed; continuing video-only";
                m_audioStream = -1;
            } else {
                m_resampler.configure(48000, 2, AV_SAMPLE_FMT_S16);
                if (!m_audio)
                    m_audio = std::make_unique<AudioOutput>();
                m_audio->open(48000, 2);
                m_audio->setVolume(m_volume);
            }
        }

        m_durationUs = m_demux.durationUs();
        m_clock.reset(0);
        m_clockAnchorSet = false;
        m_convertFailLogged = false;
        m_presentedLogged = false;
        Q_EMIT durationReady(m_durationUs / 1000);
        Q_EMIT positionChanged(0);
    }

    void play()
    {
        if (!m_demux.isOpen())
            return;
        m_stop = false;
        m_paused = false;
        m_clock.setPaused(false);
        if (m_audio)
            m_audio->resume();
        if (!m_timer->isActive())
            m_timer->start();
        Q_EMIT playingChanged(true);
    }

    void pause()
    {
        m_paused = true;
        m_clock.setPaused(true);
        if (m_audio)
            m_audio->suspend();
        Q_EMIT playingChanged(false);
    }

    void stop()
    {
        stopInternal();
        Q_EMIT playingChanged(false);
        Q_EMIT positionChanged(0);
    }

    void seek(qint64 positionMs)
    {
        if (!m_demux.isOpen() || m_demux.isLive())
            return;
        const qint64 us = positionMs * 1000;
        m_demux.seek(us);
        m_videoDecoder.flush();
        m_audioDecoder.flush();
        m_clock.reset(us);
        m_pendingVideo.reset();
        Q_EMIT positionChanged(positionMs);
    }

    void setVolume(qreal volume)
    {
        m_volume = volume;
        if (m_audio)
            m_audio->setVolume(volume);
    }

    void setVideoSink(QVideoSink *sink)
    {
        m_sink = sink;
    }

Q_SIGNALS:
    void durationReady(qint64 ms);
    void positionChanged(qint64 ms);
    void playingChanged(bool playing);
    void hardwareChanged(bool hardware);
    void error(const QString &message);
    void endOfMedia();
    void frameReady(const QVideoFrame &frame); // delivered on GUI thread via queued connect

private:
    void stopInternal()
    {
        m_timer->stop();
        m_stop = true;
        m_paused = false;
        m_demux.interrupt();
        m_demux.close();
        m_videoDecoder.close();
        m_audioDecoder.close();
        if (m_audio)
            m_audio->close();
        m_resampler.reset();
        m_pendingVideo.reset();
        m_videoStream = m_audioStream = -1;
        m_durationUs = 0;
        m_clockAnchorSet = false;
    }

    void pump()
    {
        if (m_stop || m_paused || !m_demux.isOpen())
            return;

        // Wait for a VideoOutput sink before decoding A/V — otherwise audio runs alone
        // and every video frame looks "late" → permanent black picture.
        if (m_videoStream >= 0 && !m_sink)
            return;

        // Present a pending video frame when due.
        if (m_pendingVideo) {
            const qint64 delay = m_clock.videoDelay(m_pendingVideoPtsUs);
            if (delay > 10'000)
                return; // wait
            if (delay >= -80'000 || !m_audioStream) // drop if very late (unless video-only)
                presentVideo(m_pendingVideo.get(), m_pendingVideoPtsUs);
            m_pendingVideo.reset();
        }

        PacketRef packet = makePacket();
        if (!packet)
            return;
        const MediaResult rr = m_demux.readPacket(packet.get());
        if (rr == MediaResult::Eof) {
            m_timer->stop();
            Q_EMIT endOfMedia();
            Q_EMIT playingChanged(false);
            return;
        }
        if (rr == MediaResult::Again)
            return;
        if (rr != MediaResult::Ok) {
            Q_EMIT error(m_demux.lastError());
            stopInternal();
            Q_EMIT playingChanged(false);
            return;
        }

        if (packet->stream_index == m_videoStream)
            handleVideoPacket(packet.get());
        else if (packet->stream_index == m_audioStream)
            handleAudioPacket(packet.get());
    }

    void handleVideoPacket(AVPacket *packet)
    {
        if (m_videoDecoder.send(packet) == MediaResult::Error)
            return;
        FrameRef frame = makeFrame();
        while (m_videoDecoder.receive(frame.get()) == MediaResult::Ok) {
            AVRational tb{1, AV_TIME_BASE};
            // Reconstruct from stream info.
            for (const StreamInfo &info : m_demux.streams()) {
                if (info.index == m_videoStream) {
                    tb = {info.timeBaseNum, info.timeBaseDen};
                    break;
                }
            }
            const qint64 ptsUs = ptsToUs(frame->best_effort_timestamp != AV_NOPTS_VALUE
                                              ? frame->best_effort_timestamp
                                              : frame->pts,
                                          tb);

            if (!m_audioStream) {
                // Video-only: drive clock from wall + first pts.
                if (!m_clockAnchorSet) {
                    m_clock.reset(ptsUs);
                    m_clockAnchorSet = true;
                }
            }

            const qint64 delay = m_clock.videoDelay(ptsUs);
            if (delay > 10'000) {
                m_pendingVideo = cloneFrame(frame.get());
                m_pendingVideoPtsUs = ptsUs;
                return;
            }
            if (delay < -80'000 && m_audioStream)
                continue; // drop late frame
            presentVideo(frame.get(), ptsUs);
            frame = makeFrame();
        }
    }

    void handleAudioPacket(AVPacket *packet)
    {
        if (m_audioDecoder.send(packet) == MediaResult::Error)
            return;
        FrameRef frame = makeFrame();
        while (m_audioDecoder.receive(frame.get()) == MediaResult::Ok) {
            AVRational tb{1, AV_TIME_BASE};
            for (const StreamInfo &info : m_demux.streams()) {
                if (info.index == m_audioStream) {
                    tb = {info.timeBaseNum, info.timeBaseDen};
                    break;
                }
            }
            const qint64 ptsUs = ptsToUs(frame->best_effort_timestamp != AV_NOPTS_VALUE
                                              ? frame->best_effort_timestamp
                                              : frame->pts,
                                          tb);

            QByteArray pcm;
            if (!m_resampler.convert(frame.get(), pcm) || pcm.isEmpty()) {
                frame = makeFrame();
                continue;
            }

            if (!m_audio) {
                frame = makeFrame();
                continue;
            }

            // Pace writes if the device buffer is nearly full.
            while (m_audio->freeBytes() < pcm.size() && !m_stop && !m_paused)
                QThread::msleep(2);

            m_audio->write(pcm);
            m_clock.setAudioPts(ptsUs);
            Q_EMIT positionChanged(ptsUs / 1000);
            frame = makeFrame();
        }
    }

    void presentVideo(const AVFrame *frame, qint64 ptsUs)
    {
        if (!m_sink)
            return;
        const QVideoFrame vf = m_converter.toVideoFrame(frame);
        if (!vf.isValid()) {
            if (!m_convertFailLogged) {
                m_convertFailLogged = true;
                qCWarning(lcMedia) << "Video frame convert failed (format"
                                   << (frame ? frame->format : -1) << " size"
                                   << (frame ? frame->width : 0) << "x"
                                   << (frame ? frame->height : 0)
                                   << "); picture may stay black";
            }
            return;
        }
        if (!m_presentedLogged) {
            m_presentedLogged = true;
            qCInfo(lcMedia) << "Presenting video" << vf.size() << vf.pixelFormat();
        }
        // Push to GUI thread — QVideoSink/VideoOutput are more reliable there on Windows.
        Q_EMIT frameReady(vf);
        if (!m_audioStream)
            Q_EMIT positionChanged(ptsUs / 1000);
    }

    MediaSource     m_demux;
    Decoder         m_videoDecoder;
    Decoder         m_audioDecoder;
    std::unique_ptr<AudioOutput> m_audio;
    Resampler       m_resampler;
    FrameConverter  m_converter;
    MediaClock      m_clock;
    SourceSpec      m_sourceSpec;
    QTimer         *m_timer = nullptr;
    QPointer<QVideoSink> m_sink;
    FrameRef        m_pendingVideo;
    qint64          m_pendingVideoPtsUs = 0;
    qint64          m_durationUs = 0;
    qreal           m_volume = 1.0;
    HwPolicy        m_hwPolicy = HwPolicy::Auto;
    int             m_videoStream = -1;
    int             m_audioStream = -1;
    bool            m_stop = true;
    bool            m_paused = false;
    bool            m_clockAnchorSet = false;
    bool            m_convertFailLogged = false;
    bool            m_presentedLogged = false;
};

MediaPlayer::MediaPlayer(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<qm::HwPolicy>("qm::HwPolicy");
    m_engine = new PlaybackEngine;
    m_engine->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_engine, &QObject::deleteLater);

    connect(m_engine, &PlaybackEngine::durationReady, this, [this](qint64 ms) {
        m_duration = ms;
        Q_EMIT durationChanged();
    });
    connect(m_engine, &PlaybackEngine::positionChanged, this, [this](qint64 ms) {
        if (m_position == ms)
            return;
        m_position = ms;
        Q_EMIT positionChanged();
    });
    connect(m_engine, &PlaybackEngine::playingChanged, this, [this](bool playing) {
        if (m_playing == playing)
            return;
        m_playing = playing;
        Q_EMIT playingChanged();
    });
    connect(m_engine, &PlaybackEngine::hardwareChanged, this, [this](bool hw) {
        m_hardware = hw;
        Q_EMIT decoderChanged();
    });
    connect(m_engine, &PlaybackEngine::error, this, [this](const QString &msg) {
        m_error = msg;
        Q_EMIT errorOccurred(msg);
    });
    connect(m_engine, &PlaybackEngine::endOfMedia, this, &MediaPlayer::endOfMedia);
    connect(m_engine, &PlaybackEngine::frameReady, this, [this](const QVideoFrame &frame) {
        if (m_sink)
            m_sink->setVideoFrame(frame);
    }, Qt::QueuedConnection);

    m_thread.setObjectName(QStringLiteral("PlaybackThread"));
    m_thread.start();
}

MediaPlayer::~MediaPlayer()
{
    stop();
    m_thread.quit();
    m_thread.wait(3000);
}

QUrl MediaPlayer::source() const
{
    return m_source;
}

void MediaPlayer::setSource(const QUrl &source)
{
    if (m_source == source)
        return;
    stop();
    m_source = source;
    m_error.clear();
    Q_EMIT sourceChanged();
    if (!m_source.isEmpty()) {
        QMetaObject::invokeMethod(m_engine, "open", Qt::QueuedConnection, Q_ARG(QUrl, m_source),
                                  Q_ARG(qm::HwPolicy, m_hwPolicy));
    }
}

QVideoSink *MediaPlayer::videoSink() const
{
    return m_sink;
}

void MediaPlayer::setVideoSink(QVideoSink *sink)
{
    if (m_sink == sink)
        return;
    m_sink = sink;
    QMetaObject::invokeMethod(m_engine, "setVideoSink", Qt::QueuedConnection,
                              Q_ARG(QVideoSink *, sink));
    Q_EMIT videoSinkChanged();
}

qint64 MediaPlayer::position() const
{
    return m_position;
}

qint64 MediaPlayer::duration() const
{
    return m_duration;
}

bool MediaPlayer::isPlaying() const
{
    return m_playing;
}

bool MediaPlayer::isSeekable() const
{
    return m_duration > 0;
}

qreal MediaPlayer::volume() const
{
    return m_volume;
}

void MediaPlayer::setVolume(qreal volume)
{
    volume = qBound(0.0, volume, 1.0);
    if (qFuzzyCompare(m_volume, volume))
        return;
    m_volume = volume;
    QMetaObject::invokeMethod(m_engine, "setVolume", Qt::QueuedConnection, Q_ARG(qreal, volume));
    Q_EMIT volumeChanged();
}

bool MediaPlayer::hardwareDecoding() const
{
    return m_hardware;
}

QString MediaPlayer::errorString() const
{
    return m_error;
}

void MediaPlayer::setHwPolicy(HwPolicy policy)
{
    m_hwPolicy = policy;
}

void MediaPlayer::play()
{
    if (m_source.isEmpty())
        return;
    QMetaObject::invokeMethod(m_engine, "play", Qt::QueuedConnection);
}

void MediaPlayer::pause()
{
    QMetaObject::invokeMethod(m_engine, "pause", Qt::QueuedConnection);
}

void MediaPlayer::stop()
{
    QMetaObject::invokeMethod(m_engine, "stop", Qt::QueuedConnection);
    m_playing = false;
    m_position = 0;
    Q_EMIT playingChanged();
    Q_EMIT positionChanged();
}

void MediaPlayer::seek(qint64 positionMs)
{
    QMetaObject::invokeMethod(m_engine, "seek", Qt::QueuedConnection, Q_ARG(qint64, positionMs));
}

} // namespace qm

#include "MediaPlayer.moc"
