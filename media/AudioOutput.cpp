#include "AudioOutput.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaDevices>

#include "common/Logger.h"

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
    close();
    m_sampleRate = sampleRate > 0 ? sampleRate : 48000;
    m_channels = channels > 0 ? channels : 2;

    QAudioFormat format;
    format.setSampleRate(m_sampleRate);
    format.setChannelCount(m_channels);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice dev = device;
    if (dev.isNull())
        dev = QMediaDevices::defaultAudioOutput();
    if (!dev.isFormatSupported(format)) {
        format = dev.preferredFormat();
        format.setSampleFormat(QAudioFormat::Int16);
        m_sampleRate = format.sampleRate();
        m_channels = format.channelCount();
    }

    // Do not parent the sink to this QObject: MediaPlayer may call open() from a
    // worker thread while AudioOutput still has GUI-thread affinity.
    m_sink = new QAudioSink(dev, format);
    m_io = m_sink->start();
    if (!m_io) {
        qCWarning(lcMedia) << "AudioOutput failed to start";
        close();
        return false;
    }
    return true;
}

void AudioOutput::close()
{
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
        m_sink = nullptr;
    }
    m_io = nullptr;
}

qint64 AudioOutput::write(const QByteArray &pcm)
{
    if (!m_io || pcm.isEmpty())
        return 0;
    return m_io->write(pcm);
}

qint64 AudioOutput::bufferedUs() const
{
    if (!m_sink)
        return 0;
    const qint64 bytes = m_sink->bufferSize() - m_sink->bytesFree();
    const qint64 bytesPerSec = qint64(m_sampleRate) * m_channels * 2;
    if (bytesPerSec <= 0)
        return 0;
    return bytes * 1'000'000 / bytesPerSec;
}

qint64 AudioOutput::freeBytes() const
{
    return m_sink ? m_sink->bytesFree() : 0;
}

void AudioOutput::setVolume(qreal volume)
{
    if (m_sink)
        m_sink->setVolume(qBound(0.0, volume, 1.0));
}

qreal AudioOutput::volume() const
{
    return m_sink ? m_sink->volume() : 1.0;
}

void AudioOutput::suspend()
{
    if (m_sink)
        m_sink->suspend();
}

void AudioOutput::resume()
{
    if (m_sink)
        m_sink->resume();
}

} // namespace qm
