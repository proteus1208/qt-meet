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

public Q_SLOTS:
    void start()
    {
        {
            QMutexLocker locker(&m_mutex);
            m_stop = false;
            if (!m_source.open(m_spec)) {
                Q_EMIT failed(m_source.lastError());
                return;
            }
            const MediaType type = m_isVideo ? MediaType::Video : MediaType::Audio;
            const int index = m_source.bestStream(type);
            if (index < 0) {
                Q_EMIT failed(QStringLiteral("No stream in capture source"));
                return;
            }
            m_stream = index;
            if (!m_decoder.open(m_source.codecParameters(index),
                                m_isVideo ? m_policy : HwPolicy::SoftwareOnly)) {
                Q_EMIT failed(QStringLiteral("Cannot open capture decoder"));
                return;
            }
        }
        loop();
    }

    void stop()
    {
        QMutexLocker locker(&m_mutex);
        m_stop = true;
        m_source.interrupt();
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

    if (!m_videoSpec.location.isEmpty() && m_videoEnabled) {
        m_videoThread = new QThread(this);
        m_videoWorker = new CaptureWorker(true);
        m_videoWorker->configure(m_videoSpec, HwPolicy::Auto);
        m_videoWorker->moveToThread(m_videoThread);
        connect(m_videoThread, &QThread::started, m_videoWorker, &CaptureWorker::start);
        connect(m_videoThread, &QThread::finished, m_videoWorker, &QObject::deleteLater);
        connect(m_videoWorker, &CaptureWorker::frameReady, this, [this](const FrameRef &frame) {
            Q_EMIT videoFrame(frame);
            if (m_preview && frame) {
                FrameConverter converter;
                const QVideoFrame vf = converter.toVideoFrame(frame.get());
                if (vf.isValid())
                    m_preview->setVideoFrame(vf);
            }
        });
        connect(m_videoWorker, &CaptureWorker::failed, this, &CaptureSource::errorOccurred);
        m_videoThread->start();
    }

    if (!m_audioSpec.location.isEmpty() && m_audioEnabled) {
        m_audioThread = new QThread(this);
        m_audioWorker = new CaptureWorker(false);
        m_audioWorker->configure(m_audioSpec, HwPolicy::SoftwareOnly);
        m_audioWorker->moveToThread(m_audioThread);
        connect(m_audioThread, &QThread::started, m_audioWorker, &CaptureWorker::start);
        connect(m_audioThread, &QThread::finished, m_audioWorker, &QObject::deleteLater);
        connect(m_audioWorker, &CaptureWorker::frameReady, this, &CaptureSource::audioFrame);
        connect(m_audioWorker, &CaptureWorker::failed, this, &CaptureSource::errorOccurred);
        m_audioThread->start();
    }

    m_running = true;
}

void CaptureSource::stop()
{
    if (m_videoWorker)
        QMetaObject::invokeMethod(m_videoWorker, "stop", Qt::QueuedConnection);
    if (m_audioWorker)
        QMetaObject::invokeMethod(m_audioWorker, "stop", Qt::QueuedConnection);

    if (m_videoThread) {
        m_videoThread->quit();
        m_videoThread->wait(3000);
        m_videoThread->deleteLater();
        m_videoThread = nullptr;
        m_videoWorker = nullptr;
    }
    if (m_audioThread) {
        m_audioThread->quit();
        m_audioThread->wait(3000);
        m_audioThread->deleteLater();
        m_audioThread = nullptr;
        m_audioWorker = nullptr;
    }
    m_running = false;
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
    if (width > 0)
        m_videoSpec.options.insert(QStringLiteral("video_size"),
                                   QStringLiteral("%1x%2").arg(width).arg(height));
    if (fps > 0)
        m_videoSpec.options.insert(QStringLiteral("framerate"), QString::number(fps));
}

void CaptureSource::videoLoop()
{
}

void CaptureSource::audioLoop()
{
}

} // namespace qm

#include "CaptureSource.moc"
