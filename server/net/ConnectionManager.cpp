#include "ConnectionManager.h"

#include <QMetaType>
#include <QTcpSocket>
#include <QThread>
#include <QtGlobal>

#include "NetWorker.h"
#include "SessionRegistry.h"
#include "common/Ids.h"
#include "common/Logger.h"

namespace qm {

ConnectionManager::ConnectionManager(int workerCount, const QSslConfiguration &ssl,
                                     SessionRegistry *sessions, QObject *parent)
    : QTcpServer(parent)
    , m_ssl(ssl)
    , m_sessions(sessions)
    , m_workerCount(qMax(1, workerCount))
{
    qRegisterMetaType<qm::ConnId>("qm::ConnId");
    qRegisterMetaType<qm::proto::Envelope>("qm::proto::Envelope");
}

ConnectionManager::~ConnectionManager()
{
    stop();
}

bool ConnectionManager::start(const QHostAddress &address, quint16 port)
{
    if (isListening())
        return true;

    for (int i = 0; i < m_workerCount; ++i) {
        auto *thread = new QThread(this);
        auto *worker = new NetWorker(m_ssl);
        worker->moveToThread(thread);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);

        connect(worker, &NetWorker::envelopeReceived, this, &ConnectionManager::envelopeReceived);
        connect(worker, &NetWorker::connectionOpened, this,
                [this, worker](ConnId id, const QHostAddress &peer) {
                    if (m_sessions)
                        m_sessions->registerConnection(id, worker);
                    Q_EMIT connectionOpened(id, peer);
                });
        connect(worker, &NetWorker::connectionClosed, this, [this](ConnId id) {
            if (m_sessions)
                m_sessions->unregisterConnection(id);
            Q_EMIT connectionClosed(id);
        });

        thread->start();
        m_threads.append(thread);
        m_workers.append(worker);
    }

    if (!listen(address, port)) {
        qCWarning(lcNet) << "listen failed:" << errorString();
        stop();
        return false;
    }

    qCInfo(lcNet) << "Listening on" << address.toString() << port << "workers" << m_workerCount;
    return true;
}

void ConnectionManager::stop()
{
    close();

    for (NetWorker *worker : std::as_const(m_workers)) {
        if (!worker)
            continue;
        QMetaObject::invokeMethod(worker, "shutdown", Qt::BlockingQueuedConnection);
    }

    for (QThread *thread : std::as_const(m_threads)) {
        thread->quit();
        thread->wait(3000);
    }
    m_workers.clear();
    qDeleteAll(m_threads);
    m_threads.clear();
}

int ConnectionManager::connectionCount() const
{
    int total = 0;
    for (const NetWorker *worker : m_workers)
        total += worker ? worker->connectionCount() : 0;
    return total;
}

void ConnectionManager::incomingConnection(qintptr socketDescriptor)
{
    NetWorker *worker = pickWorker();
    if (!worker) {
        QTcpSocket tmp;
        tmp.setSocketDescriptor(socketDescriptor);
        tmp.abort();
        return;
    }

    const ConnId id = Ids::nextConnId();
    QMetaObject::invokeMethod(worker, "adoptSocket", Qt::QueuedConnection,
                              Q_ARG(qintptr, socketDescriptor),
                              Q_ARG(qm::ConnId, id));
}

NetWorker *ConnectionManager::pickWorker()
{
    if (m_workers.isEmpty())
        return nullptr;

    NetWorker *best = m_workers.first();
    int bestCount = best->connectionCount();
    for (int i = 1; i < m_workers.size(); ++i) {
        const int c = m_workers[i]->connectionCount();
        if (c < bestCount) {
            best = m_workers[i];
            bestCount = c;
        }
    }
    return best;
}

} // namespace qm
