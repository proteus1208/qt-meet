#include "AudioOutput.h"

namespace qm {

AudioOutput::AudioOutput(QObject *parent)
    : QObject(parent)
{
}

AudioOutput::~AudioOutput()
{
    close();
}

bool AudioOutput::open(int sampleRate, int channels, const QAudioDevice &device)
{
    Q_UNUSED(device)
    m_sampleRate = sampleRate;
    m_channels = channels;
    return false;
}

void AudioOutput::close()
{
}

qint64 AudioOutput::write(const QByteArray &pcm)
{
    Q_UNUSED(pcm)
    return 0;
}

qint64 AudioOutput::bufferedUs() const
{
    return 0;
}

qint64 AudioOutput::freeBytes() const
{
    return 0;
}

void AudioOutput::setVolume(qreal volume)
{
    Q_UNUSED(volume)
}

qreal AudioOutput::volume() const
{
    return 1.0;
}

void AudioOutput::suspend()
{
}

void AudioOutput::resume()
{
}

} // namespace qm
