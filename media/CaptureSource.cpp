#include "CaptureSource.h"

#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>

#include "Decoder.h"
#include "FrameConverter.h"
#include "MediaSource.h"
#include "common/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace qm {

class CaptureWorker : public QObject
{
    Q_OBJECT
public:
    explicit CaptureWorker(bool video, QObject *parent = nullptr)
        : QObject(parent)
        , m_isVideo(video)
    {
    }

    void configure(const SourceSpec &spec, HwPolicy policy)
    {
        QMutexLocker locker(&m_mutex);
        m_spec = spec;
        m_policy = policy;
    }

    /// Thread-safe: call from any thread to unblock gdigrab / dshow I/O.
    void requestStop()
    {
        QMutexLocker locker(&m_mutex);
        m_stop = true;
        m_source.interrupt();
    }

public Q_SLOTS:
    void start()
    {
        {
            QMutexLocker locker(&m_mutex);
            m_stop = false;
            qCInfo(lcMedia) << "CaptureWorker open" << (m_isVideo ? "video" : "audio")
                            << m_spec.format << m_spec.location << m_spec.displayName;
            if (!m_source.open(m_spec)) {
                qCWarning(lcMedia) << "CaptureWorker open failed:" << m_source.lastError()
                                   << m_spec.format << m_spec.location;
                Q_EMIT failed(m_source.lastError());
                return;
            }
            const MediaType type = m_isVideo ? MediaType::Video : MediaType::Audio;
            const int index = m_source.bestStream(type);
            if (index < 0) {
                qCWarning(lcMedia) << "CaptureWorker: no stream in source" << m_spec.location;
                Q_EMIT failed(QStringLiteral("No stream in capture source"));
                return;
            }
            m_stream = index;
            if (!m_decoder.open(m_source.codecParameters(index),
                                m_isVideo ? m_policy : HwPolicy::SoftwareOnly)) {
                qCWarning(lcMedia) << "CaptureWorker: decoder open failed" << m_spec.location;
                Q_EMIT failed(QStringLiteral("Cannot open capture decoder"));
                return;
            }
            qCInfo(lcMedia) << "CaptureWorker running stream" << index << m_spec.location;
        }
        loop();
    }

    void stop()
    {
        requestStop();
    }

Q_SIGNALS:
    void frameReady(const qm::FrameRef &frame);
    void failed(const QString &message);

private:
    void loop()
    {
        while (true) {
            {
                QMutexLocker locker(&m_mutex);
                if (m_stop)
                    break;
            }
            PacketRef packet = makePacket();
            if (!packet)
                break;
            const MediaResult rr = m_source.readPacket(packet.get());
            if (rr == MediaResult::Eof) {
                if (m_spec.kind == SourceKind::File) {
                    m_source.seek(0);
                    m_decoder.flush();
                    continue;
                }
                break;
            }
            if (rr != MediaResult::Ok)
                continue;
            if (packet->stream_index != m_stream)
                continue;

            if (m_decoder.send(packet.get()) != MediaResult::Ok)
                continue;
            FrameRef frame = makeFrame();
            while (m_decoder.receive(frame.get()) == MediaResult::Ok) {
                Q_EMIT frameReady(cloneFrame(frame.get()));
                frame = makeFrame();
            }
        }
        m_decoder.close();
        m_source.close();
    }

    MediaSource m_source;
    Decoder     m_decoder;
    SourceSpec  m_spec;
    HwPolicy    m_policy = HwPolicy::Auto;
    QMutex      m_mutex;
    int         m_stream = -1;
    bool        m_isVideo = true;
    bool        m_stop = true;
};

CaptureSource::CaptureSource(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<qm::FrameRef>("qm::FrameRef");
}

CaptureSource::~CaptureSource()
{
    stop();
}

bool CaptureSource::open(const SourceSpec &video, const SourceSpec &audio)
{
    stop();
    m_videoSpec = video;
    m_audioSpec = audio;
    return !video.location.isEmpty() || !audio.location.isEmpty();
}

void CaptureSource::start()
{
    if (m_running)
        return;
    startVideo();
    startAudio();
    m_running = true;
}

void CaptureSource::stop()
{
    stopVideo();
    stopAudio();
    m_running = false;
}

bool CaptureSource::isRunning() const
{
    return m_running;
}

bool CaptureSource::reopenVideo(const SourceSpec &video, bool enableVideo)
{
    m_videoSpec = video;
    if (enableVideo)
        m_videoEnabled = true;

    qCInfo(lcMedia) << "reopenVideo" << video.format << video.location
                    << "running" << m_running << "videoEnabled" << m_videoEnabled
                    << "preview" << (m_preview != nullptr);

    if (video.location.isEmpty()) {
        qCWarning(lcMedia) << "reopenVideo: empty location";
        stopVideo();
        return false;
    }
    if (!m_running)
        return true;

    stopVideo();
    if (m_videoEnabled)
        startVideo();
    return m_videoThread != nullptr;
}

bool CaptureSource::reopenAudio(const SourceSpec &audio)
{
    m_audioSpec = audio;
    if (!m_running)
        return !audio.location.isEmpty();
    stopAudio();
    if (m_audioEnabled)
        startAudio();
    return true;
}

void CaptureSource::setVideoEnabled(bool enabled)
{
    if (m_videoEnabled == enabled)
        return;
    m_videoEnabled = enabled;
    if (!m_running)
        return;
    if (enabled)
        startVideo();
    else
        stopVideo();
}

void CaptureSource::setAudioEnabled(bool enabled)
{
    if (m_audioEnabled == enabled)
        return;
    m_audioEnabled = enabled;
    if (!m_running)
        return;
    if (enabled)
        startAudio();
    else
        stopAudio();
}

void CaptureSource::setPreviewSink(QVideoSink *sink)
{
    m_preview = sink;
}

void CaptureSource::setOutputSize(int width, int height, int fps)
{
    if (width > 0)
        m_videoSpec.options.insert(QStringLiteral("video_size"),
                                   QStringLiteral("%1x%2").arg(width).arg(height));
    if (fps > 0)
        m_videoSpec.options.insert(QStringLiteral("framerate"), QString::number(fps));
}

SourceSpec CaptureSource::videoSpec() const
{
    return m_videoSpec;
}

SourceSpec CaptureSource::audioSpec() const
{
    return m_audioSpec;
}

void CaptureSource::joinThread(QThread *&thread, CaptureWorker *&worker, const char *label)
{
    if (!thread)
        return;

    // loop() blocks the worker thread event loop, so QueuedConnection stop never runs.
    // Interrupt FFmpeg I/O directly from this thread.
    if (worker)
        worker->requestStop();

    thread->quit();
    for (int i = 0; i < 80 && thread->isRunning(); ++i) {
        if (worker)
            worker->requestStop();
        if (thread->wait(100))
            break;
    }
    if (thread->isRunning()) {
        qCWarning(lcMedia) << label << "capture thread still running; terminating";
        thread->terminate();
        thread->wait(2000);
    }

    // Never parent QThread to CaptureSource — ~QObject would destroy it while running.
    thread->setParent(nullptr);
    delete thread;
    thread = nullptr;
    worker = nullptr; // deleted via finished → deleteLater (or already gone after terminate)
}

void CaptureSource::stopVideo()
{
    joinThread(m_videoThread, m_videoWorker, "video");
}

void CaptureSource::stopAudio()
{
    joinThread(m_audioThread, m_audioWorker, "audio");
}

void CaptureSource::startVideo()
{
    if (m_videoThread || m_videoSpec.location.isEmpty() || !m_videoEnabled) {
        qCInfo(lcMedia) << "startVideo skipped thread" << (m_videoThread != nullptr)
                        << "locEmpty" << m_videoSpec.location.isEmpty()
                        << "enabled" << m_videoEnabled;
        return;
    }

    qCInfo(lcMedia) << "startVideo" << m_videoSpec.format << m_videoSpec.location
                    << m_videoSpec.displayName << "preview" << (m_preview != nullptr);

    m_videoThread = new QThread; // no parent — joined explicitly in stopVideo()
    m_videoThread->setObjectName(QStringLiteral("CaptureVideo"));
    m_videoWorker = new CaptureWorker(true);
    m_videoWorker->configure(m_videoSpec, HwPolicy::SoftwareOnly);
    m_videoWorker->moveToThread(m_videoThread);
    connect(m_videoThread, &QThread::started, m_videoWorker, &CaptureWorker::start);
    connect(m_videoThread, &QThread::finished, m_videoWorker, &QObject::deleteLater);
    connect(m_videoWorker, &CaptureWorker::frameReady, this, [this](const FrameRef &frame) {
        Q_EMIT videoFrame(frame);
        if (!m_preview || !frame)
            return;
        FrameConverter converter;
        const QVideoFrame vf = converter.toVideoFrame(frame.get());
        if (!vf.isValid()) {
            static int convertFails = 0;
            if ((convertFails++ % 60) == 0)
                qCWarning(lcMedia) << "preview convert failed format"
                                   << (frame ? frame->format : -1) << "count" << convertFails;
            return;
        }
        m_preview->setVideoFrame(vf);
        static int previewFrames = 0;
        if ((previewFrames++ % 60) == 0)
            qCInfo(lcMedia) << "local preview frames" << previewFrames << vf.size();
    });
    connect(m_videoWorker, &CaptureWorker::failed, this, [this](const QString &message) {
        qCWarning(lcMedia) << "video capture failed:" << message;
        Q_EMIT errorOccurred(message);
    });
    m_videoThread->start();
}

void CaptureSource::startAudio()
{
    if (m_audioThread || m_audioSpec.location.isEmpty() || !m_audioEnabled)
        return;

    m_audioThread = new QThread;
    m_audioThread->setObjectName(QStringLiteral("CaptureAudio"));
    m_audioWorker = new CaptureWorker(false);
    m_audioWorker->configure(m_audioSpec, HwPolicy::SoftwareOnly);
    m_audioWorker->moveToThread(m_audioThread);
    connect(m_audioThread, &QThread::started, m_audioWorker, &CaptureWorker::start);
    connect(m_audioThread, &QThread::finished, m_audioWorker, &QObject::deleteLater);
    connect(m_audioWorker, &CaptureWorker::frameReady, this, &CaptureSource::audioFrame);
    connect(m_audioWorker, &CaptureWorker::failed, this, &CaptureSource::errorOccurred);
    m_audioThread->start();
}

} // namespace qm

#include "CaptureSource.moc"
