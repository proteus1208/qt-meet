#include "SessionRegistry.h"

#include "NetWorker.h"

namespace qm {

SessionRegistry::SessionRegistry(QObject *parent)
    : QObject(parent)
{
}

SessionRegistry::~SessionRegistry() = default;

void SessionRegistry::registerConnection(ConnId conn, NetWorker *worker)
{
    Q_UNUSED(conn)
    Q_UNUSED(worker)
}

void SessionRegistry::unregisterConnection(ConnId conn)
{
    Q_UNUSED(conn)
}

bool SessionRegistry::bind(ConnId conn, UserId user)
{
    Q_UNUSED(conn)
    Q_UNUSED(user)
    return false;
}

void SessionRegistry::unbind(ConnId conn)
{
    Q_UNUSED(conn)
}

UserId SessionRegistry::userOf(ConnId conn) const
{
    Q_UNUSED(conn)
    return 0;
}

QList<ConnId> SessionRegistry::connectionsOf(UserId user) const
{
    Q_UNUSED(user)
    return {};
}

bool SessionRegistry::isOnline(UserId user) const
{
    Q_UNUSED(user)
    return false;
}

QList<UserId> SessionRegistry::onlineUsers() const
{
    return {};
}

void SessionRegistry::sendToConn(ConnId conn, const proto::Envelope &envelope)
{
    Q_UNUSED(conn)
    Q_UNUSED(envelope)
}

void SessionRegistry::sendTo(UserId user, const proto::Envelope &envelope)
{
    Q_UNUSED(user)
    Q_UNUSED(envelope)
}

void SessionRegistry::broadcast(const QList<UserId> &users, const proto::Envelope &envelope)
{
    Q_UNUSED(users)
    Q_UNUSED(envelope)
}

void SessionRegistry::broadcastAll(const proto::Envelope &envelope)
{
    Q_UNUSED(envelope)
}

} // namespace qm
