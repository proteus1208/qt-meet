#include "AudioProcessor.h"

namespace qm {

AudioProcessor::AudioProcessor() = default;

AudioProcessor::~AudioProcessor() = default;

bool AudioProcessor::init(const Settings &settings)
{
    m_settings = settings;
    return false;
}

void AudioProcessor::processRender(qint16 *samples, int frames)
{
    Q_UNUSED(samples)
    Q_UNUSED(frames)
}

void AudioProcessor::processCapture(qint16 *samples, int frames)
{
    Q_UNUSED(samples)
    Q_UNUSED(frames)
}

void AudioProcessor::setStreamDelayMs(int delayMs)
{
    m_delayMs = delayMs;
}

bool AudioProcessor::isActive() const
{
    return m_apm != nullptr;
}

int AudioProcessor::frameSamples() const
{
    return m_settings.sampleRate / 100;
}

} // namespace qm
