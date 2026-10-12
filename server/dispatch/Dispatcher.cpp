#include "Dispatcher.h"

#include "net/SessionRegistry.h"

namespace qm {

Dispatcher::Dispatcher(SessionRegistry *sessions, int threadCount, QObject *parent)
    : QObject(parent)
    , m_sessions(sessions)
{
    Q_UNUSED(threadCount)
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
}

void Dispatcher::dispatch(ConnId conn, const proto::Envelope &envelope)
{
    Q_UNUSED(conn)
    Q_UNUSED(envelope)
}

void Dispatcher::onConnectionOpened(ConnId conn, const QHostAddress &peer)
{
    Q_UNUSED(conn)
    Q_UNUSED(peer)
}

void Dispatcher::onConnectionClosed(ConnId conn)
{
    Q_UNUSED(conn)
}

void Dispatcher::drain(ConnId conn)
{
    Q_UNUSED(conn)
}

void Dispatcher::rejectUnauthenticated(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

} // namespace qm
