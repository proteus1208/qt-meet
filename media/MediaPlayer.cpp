#include "MediaPlayer.h"

namespace qm {

MediaPlayer::MediaPlayer(QObject *parent)
    : QObject(parent)
{
}

MediaPlayer::~MediaPlayer()
{
    m_thread.quit();
    m_thread.wait();
}

QUrl MediaPlayer::source() const
{
    return m_source;
}

void MediaPlayer::setSource(const QUrl &source)
{
    if (m_source == source)
        return;
    m_source = source;
    Q_EMIT sourceChanged();
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
    if (qFuzzyCompare(m_volume, volume))
        return;
    m_volume = volume;
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
}

void MediaPlayer::pause()
{
}

void MediaPlayer::stop()
{
}

void MediaPlayer::seek(qint64 positionMs)
{
    Q_UNUSED(positionMs)
}

} // namespace qm
