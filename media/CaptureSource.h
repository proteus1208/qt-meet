#pragma once

#include <QObject>
#include <QPointer>
#include <QThread>
#include <QVideoSink>

#include "MediaTypes.h"

namespace qm {

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

    void setVideoEnabled(bool enabled);
    void setAudioEnabled(bool enabled);
    void setPreviewSink(QVideoSink *sink);
    void setOutputSize(int width, int height, int fps);   // applied when the device allows it

Q_SIGNALS:
    void videoFrame(const qm::FrameRef &frame);
    void audioFrame(const qm::FrameRef &frame);
    void errorOccurred(const QString &message);

private:
    void videoLoop();
    void audioLoop();

    SourceSpec           m_videoSpec;
    SourceSpec           m_audioSpec;
    QThread             *m_videoThread = nullptr;
    QThread             *m_audioThread = nullptr;
    QPointer<QVideoSink> m_preview;
    bool                 m_running = false;
    bool                 m_videoEnabled = true;
    bool                 m_audioEnabled = true;
};

} // namespace qm
