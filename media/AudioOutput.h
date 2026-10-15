#pragma once

#include <QAudioDevice>
#include <QObject>

class QAudioSink;
class QIODevice;

namespace qm {

/// Aim: PCM playback through QAudioSink; reports buffered latency so MediaClock can be driven
/// by what is actually audible. Also acts as the render reference for echo cancellation.
class AudioOutput : public QObject
{
    Q_OBJECT

public:
    explicit AudioOutput(QObject *parent = nullptr);
    ~AudioOutput() override;

    bool open(int sampleRate, int channels, const QAudioDevice &device = {});
    void close();

    qint64 write(const QByteArray &pcm);      // interleaved int16
    qint64 bufferedUs() const;
    qint64 freeBytes() const;

    void  setVolume(qreal volume);
    qreal volume() const;
    void  suspend();
    void  resume();

Q_SIGNALS:
    void underrun();

private:
    QAudioSink *m_sink = nullptr;
    QIODevice  *m_io = nullptr;
    int         m_sampleRate = 48000;
    int         m_channels = 2;
};

} // namespace qm
