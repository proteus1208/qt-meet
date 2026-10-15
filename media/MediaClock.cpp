#include "MediaClock.h"

namespace qm {

void MediaClock::reset(qint64 startUs)
{
    QMutexLocker locker(&m_mutex);
    m_audioPts = startUs;
    m_paused = false;
    m_sinceAudio.invalidate();
}

void MediaClock::setAudioPts(qint64 ptsUs)
{
    QMutexLocker locker(&m_mutex);
    m_audioPts = ptsUs;
    m_sinceAudio.restart();
}

qint64 MediaClock::now() const
{
    QMutexLocker locker(&m_mutex);
    if (m_paused || !m_sinceAudio.isValid())
        return m_audioPts;
    return m_audioPts + qint64(m_sinceAudio.nsecsElapsed() / 1000.0 * m_speed);
}

qint64 MediaClock::videoDelay(qint64 videoPtsUs) const
{
    return videoPtsUs - now();
}

void MediaClock::setPaused(bool paused)
{
    QMutexLocker locker(&m_mutex);
    if (m_paused == paused)
        return;
    if (paused) {
        if (m_sinceAudio.isValid())
            m_audioPts += qint64(m_sinceAudio.nsecsElapsed() / 1000.0 * m_speed);
        m_sinceAudio.invalidate();
    } else {
        m_sinceAudio.restart();
    }
    m_paused = paused;
}

bool MediaClock::isPaused() const
{
    QMutexLocker locker(&m_mutex);
    return m_paused;
}

void MediaClock::setSpeed(double speed)
{
    QMutexLocker locker(&m_mutex);
    m_speed = speed > 0.0 ? speed : 1.0;
}

} // namespace qm
