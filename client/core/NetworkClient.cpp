#include "NetworkClient.h"

#include <QTimer>

#include <stdexcept>

#include "common/EnvelopeFactory.h"
#include "common/Logger.h"

#include "NetworkWorker.h"

namespace qm {

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<qm::proto::Envelope>("qm::proto::Envelope");
}

NetworkClient::~NetworkClient()
{
    shutdown();
}

void NetworkClient::start()
{
    if (m_thread.isRunning())
        return;

    m_worker = new NetworkWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &NetworkWorker::encrypted, this, [this] { setState(State::Connected); });
    connect(m_worker, &NetworkWorker::disconnected, this, [this] {
        failPending(QStringLiteral("Connection closed"));
        setState(State::Disconnected);
    });
    connect(m_worker, &NetworkWorker::error, this, &NetworkClient::errorOccurred);
    connect(m_worker, &NetworkWorker::envelope, this, &NetworkClient::onWorkerFrame);

    m_thread.start();
}

void NetworkClient::shutdown()
{
    disconnectFromServer();
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait(3000);
    }
    m_worker = nullptr;
}

void NetworkClient::connectToServer(const QString &host, quint16 port, const QString &caFile)
{
    m_host = host;
    m_port = port;
    m_caFile = caFile;
    start();
    setState(State::Connecting);
    QMetaObject::invokeMethod(m_worker, "connectTo", Qt::QueuedConnection,
                              Q_ARG(QString, host),
                              Q_ARG(quint16, port));
}

void NetworkClient::disconnectFromServer()
{
    failPending(QStringLiteral("Disconnected"));
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "disconnectHost", Qt::QueuedConnection);
    }
    setState(State::Disconnected);
}

QFuture<proto::Envelope> NetworkClient::request(proto::Envelope envelope, int timeoutMs)
{
    if (envelope.seq() == 0)
        envelope.set_seq(EnvelopeFactory::nextSeq());

    auto promise = std::make_shared<QPromise<proto::Envelope>>();
    promise->start();
    const quint64 seq = envelope.seq();
    {
        QMutexLocker locker(&m_pendingMutex);
        m_pending.insert(seq, promise);
    }

    QTimer::singleShot(timeoutMs, this, [this, seq] {
        QMutexLocker locker(&m_pendingMutex);
        if (auto p = m_pending.take(seq)) {
            p->setException(std::make_exception_ptr(std::runtime_error("request timeout")));
            p->finish();
        }
    });

    post(envelope);
    return promise->future();
}

void NetworkClient::post(proto::Envelope envelope)
{
    if (envelope.seq() == 0)
        envelope.set_seq(EnvelopeFactory::nextSeq());

    if (!m_worker) {
        Q_EMIT errorOccurred(QStringLiteral("Network not started"));
        return;
    }

    const QByteArray frame = FrameCodec::encode(envelope);
    QMetaObject::invokeMethod(m_worker, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(QByteArray, frame));
}

NetworkClient::State NetworkClient::state() const
{
    return m_state.load();
}

void NetworkClient::onWorkerFrame(const proto::Envelope &envelope)
{
    if (envelope.reply_to() != 0) {
        std::shared_ptr<QPromise<proto::Envelope>> promise;
        {
            QMutexLocker locker(&m_pendingMutex);
            promise = m_pending.take(envelope.reply_to());
        }
        if (promise) {
            promise->addResult(envelope);
            promise->finish();
        }
        return;
    }
    Q_EMIT event(envelope);
}

void NetworkClient::failPending(const QString &reason)
{
    QMutexLocker locker(&m_pendingMutex);
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        it.value()->setException(std::make_exception_ptr(std::runtime_error(reason.toStdString())));
        it.value()->finish();
    }
    m_pending.clear();
}

void NetworkClient::setState(State state)
{
    if (m_state.exchange(state) == state)
        return;
    Q_EMIT stateChanged(state);
}

} // namespace qm
