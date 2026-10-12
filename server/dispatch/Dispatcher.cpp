#include "Dispatcher.h"

#include "common/EnvelopeFactory.h"
#include "common/Logger.h"
#include "net/SessionRegistry.h"

namespace qm {

Dispatcher::Dispatcher(SessionRegistry *sessions, int threadCount, QObject *parent)
    : QObject(parent)
    , m_sessions(sessions)
{
    m_pool.setMaxThreadCount(qMax(1, threadCount));
    qRegisterMetaType<qm::ConnId>("qm::ConnId");
    qRegisterMetaType<qm::proto::Envelope>("qm::proto::Envelope");
}

Dispatcher::~Dispatcher()
{
    shutdown();
}

void Dispatcher::registerHandler(proto::Envelope::BodyCase bodyCase, IHandler *handler)
{
    m_handlers.insert(static_cast<int>(bodyCase), handler);
}

void Dispatcher::shutdown()
{
    m_pool.waitForDone(5000);
    QMutexLocker locker(&m_queuesMutex);
    m_queues.clear();
}

void Dispatcher::dispatch(ConnId conn, const proto::Envelope &envelope)
{
    QMutexLocker locker(&m_queuesMutex);
    ConnQueue &queue = m_queues[conn];
    queue.pending.enqueue(envelope);
    if (!queue.running) {
        queue.running = true;
        locker.unlock();
        drain(conn);
    }
}

void Dispatcher::onConnectionOpened(ConnId conn, const QHostAddress &peer)
{
    QMutexLocker locker(&m_queuesMutex);
    ConnQueue &queue = m_queues[conn];
    queue.peer = peer;
    queue.running = false;
    queue.pending.clear();
}

void Dispatcher::onConnectionClosed(ConnId conn)
{
    QMutexLocker locker(&m_queuesMutex);
    m_queues.remove(conn);
}

void Dispatcher::drain(ConnId conn)
{
    proto::Envelope envelope;
    QHostAddress peer;
    {
        QMutexLocker locker(&m_queuesMutex);
        auto it = m_queues.find(conn);
        if (it == m_queues.end() || it->pending.isEmpty()) {
            if (it != m_queues.end())
                it->running = false;
            return;
        }
        envelope = it->pending.dequeue();
        peer = it->peer;
    }

    RequestContext context;
    context.conn = conn;
    context.peer = peer;
    context.user = m_sessions ? m_sessions->userOf(conn) : 0;

    IHandler *handler = m_handlers.value(static_cast<int>(envelope.body_case()), nullptr);
    if (!handler) {
        if (m_sessions) {
            m_sessions->sendToConn(conn, EnvelopeFactory::error(
                                             envelope, proto::ERR_INVALID,
                                             QStringLiteral("Unhandled message")));
        }
        QMetaObject::invokeMethod(this, [this, conn] { drain(conn); }, Qt::QueuedConnection);
        return;
    }

    if (handler->requiresAuth() && !context.authenticated()) {
        rejectUnauthenticated(context, envelope);
        QMetaObject::invokeMethod(this, [this, conn] { drain(conn); }, Qt::QueuedConnection);
        return;
    }

    m_pool.start([this, handler, context, envelope, conn]() {
        handler->handle(context, envelope);
        QMetaObject::invokeMethod(this, [this, conn] { drain(conn); }, Qt::QueuedConnection);
    });
}

void Dispatcher::rejectUnauthenticated(const RequestContext &context, const proto::Envelope &envelope)
{
    if (!m_sessions)
        return;
    m_sessions->sendToConn(context.conn,
                           EnvelopeFactory::error(envelope, proto::ERR_UNAUTHORIZED,
                                                  QStringLiteral("Login required")));
}

} // namespace qm
