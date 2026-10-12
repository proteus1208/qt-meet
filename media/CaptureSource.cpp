#include "CaptureSource.h"

namespace qm {

CaptureSource::CaptureSource(QObject *parent)
    : QObject(parent)
{
}

CaptureSource::~CaptureSource()
{
    stop();
}

bool CaptureSource::open(const SourceSpec &video, const SourceSpec &audio)
{
    m_videoSpec = video;
    m_audioSpec = audio;
    return false;
}

void CaptureSource::start()
{
}

void CaptureSource::stop()
{
}

bool CaptureSource::isRunning() const
{
    return m_running;
}

void CaptureSource::setVideoEnabled(bool enabled)
{
    m_videoEnabled = enabled;
}

void CaptureSource::setAudioEnabled(bool enabled)
{
    m_audioEnabled = enabled;
}

void CaptureSource::setPreviewSink(QVideoSink *sink)
{
    m_preview = sink;
}

void CaptureSource::setOutputSize(int width, int height, int fps)
{
    Q_UNUSED(width)
    Q_UNUSED(height)
    Q_UNUSED(fps)
}

void CaptureSource::videoLoop()
{
}

void CaptureSource::audioLoop()
{
}

} // namespace qm
