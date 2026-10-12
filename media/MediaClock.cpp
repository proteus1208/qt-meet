#include "MediaClock.h"

namespace qm {

void MediaClock::reset(qint64 startUs)
{
    Q_UNUSED(startUs)
}

void MediaClock::setAudioPts(qint64 ptsUs)
{
    Q_UNUSED(ptsUs)
}

qint64 MediaClock::now() const
{
    return 0;
}

qint64 MediaClock::videoDelay(qint64 videoPtsUs) const
{
    Q_UNUSED(videoPtsUs)
    return 0;
}

void MediaClock::setPaused(bool paused)
{
    Q_UNUSED(paused)
}

bool MediaClock::isPaused() const
{
    return m_paused;
}

void MediaClock::setSpeed(double speed)
{
    Q_UNUSED(speed)
}

} // namespace qm
