#pragma once

#include <QObject>
#include <QPointer>
#include <QThread>
#include <QVideoSink>

#include "MediaTypes.h"

namespace qm {

class CaptureWorker;

/// Aim: live input (local camera, microphone, RTSP IP camera, file loop) producing raw frames
/// for the Encoder, plus an optional local preview sink.
class CaptureSource : public QObject
{
    Q_OBJECT

public:
    explicit CaptureSource(QObject *parent = nullptr);
    ~CaptureSource() override;

    bool open(const SourceSpec &video, const SourceSpec &audio);
    void start();
    void stop();
    bool isRunning() const;

    /// Hot-swap the live video device (camera ↔ screen) without touching audio.
    bool reopenVideo(const SourceSpec &video);
    /// Hot-swap the live microphone.
    bool reopenAudio(const SourceSpec &audio);

    void setVideoEnabled(bool enabled);
    void setAudioEnabled(bool enabled);
    void setPreviewSink(QVideoSink *sink);
    void setOutputSize(int width, int height, int fps);   // applied when the device allows it

    SourceSpec videoSpec() const;
    SourceSpec audioSpec() const;

Q_SIGNALS:
    void videoFrame(const qm::FrameRef &frame);
    void audioFrame(const qm::FrameRef &frame);
    void errorOccurred(const QString &message);

private:
    void stopVideo();
    void stopAudio();
    void startVideo();
    void startAudio();

    SourceSpec           m_videoSpec;
    SourceSpec           m_audioSpec;
    QThread             *m_videoThread = nullptr;
    QThread             *m_audioThread = nullptr;
    CaptureWorker       *m_videoWorker = nullptr;
    CaptureWorker       *m_audioWorker = nullptr;
    QPointer<QVideoSink> m_preview;
    bool                 m_running = false;
    bool                 m_videoEnabled = true;
    bool                 m_audioEnabled = true;
};

} // namespace qm
