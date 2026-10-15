#pragma once

#include <QElapsedTimer>
#include <QMutex>

namespace qm {

/// Aim: audio-master A/V synchronization clock shared by the audio and video render paths.
class MediaClock
{
public:
    void reset(qint64 startUs = 0);

    void   setAudioPts(qint64 ptsUs);
    qint64 now() const;                       // current master position in microseconds

    /// Microseconds to wait before presenting a video frame; negative = late, drop it.
    qint64 videoDelay(qint64 videoPtsUs) const;

    void   setPaused(bool paused);
    bool   isPaused() const;
    void   setSpeed(double speed);

private:
    mutable QMutex m_mutex;
    QElapsedTimer  m_sinceAudio;
    qint64         m_audioPts = 0;
    bool           m_paused = false;
    double         m_speed = 1.0;
};

} // namespace qm
