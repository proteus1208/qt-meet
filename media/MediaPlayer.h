#pragma once

#include <QObject>
#include <QPointer>
#include <QThread>
#include <QUrl>
#include <QVideoSink>

#include "MediaTypes.h"

namespace qm {

class PlaybackEngine;

/// Aim: QML-facing player for files, HTTP (hosted folder) and RTSP sources.
/// Demux and decode run on worker threads; video is pushed to a QVideoSink, audio to
/// AudioOutput, synchronized by MediaClock.
class MediaPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QVideoSink *videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool seekable READ isSeekable NOTIFY durationChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool hardwareDecoding READ hardwareDecoding NOTIFY decoderChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorOccurred)

public:
    explicit MediaPlayer(QObject *parent = nullptr);
    ~MediaPlayer() override;

    QUrl        source() const;
    void        setSource(const QUrl &source);
    QVideoSink *videoSink() const;
    void        setVideoSink(QVideoSink *sink);
    qint64      position() const;                // ms
    qint64      duration() const;                // ms
    bool        isPlaying() const;
    bool        isSeekable() const;
    qreal       volume() const;
    void        setVolume(qreal volume);
    bool        hardwareDecoding() const;
    QString     errorString() const;

    void setHwPolicy(HwPolicy policy);

public Q_SLOTS:
    void play();
    void pause();
    void stop();
    void seek(qint64 positionMs);

Q_SIGNALS:
    void sourceChanged();
    void videoSinkChanged();
    void positionChanged();
    void durationChanged();
    void playingChanged();
    void volumeChanged();
    void decoderChanged();
    void errorOccurred(const QString &message);
    void endOfMedia();

private:
    QUrl                 m_source;
    QPointer<QVideoSink> m_sink;
    PlaybackEngine      *m_engine = nullptr;
    QThread              m_thread;
    HwPolicy             m_hwPolicy = HwPolicy::Auto;
    qint64               m_position = 0;
    qint64               m_duration = 0;
    qreal                m_volume = 1.0;
    bool                 m_playing = false;
    bool                 m_hardware = false;
    QString              m_error;
};

} // namespace qm
