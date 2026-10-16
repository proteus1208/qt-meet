#include "SessionRegistry.h"

#include "NetWorker.h"
#include "common/Logger.h"

namespace qm {

SessionRegistry::SessionRegistry(QObject *parent)
    : QObject(parent)
{
}

SessionRegistry::~SessionRegistry() = default;

void SessionRegistry::registerConnection(ConnId conn, NetWorker *worker)
{
    QWriteLocker locker(&m_lock);
    m_workers.insert(conn, worker);
}

void SessionRegistry::unregisterConnection(ConnId conn)
{
    UserId user = 0;
    {
        QWriteLocker locker(&m_lock);
        m_workers.remove(conn);
        user = m_connUser.take(conn);
        if (user != 0) {
            m_userConns.remove(user, conn);
            if (!m_userConns.contains(user)) {
                locker.unlock();
                Q_EMIT userOffline(user);
                return;
            }
        }
    }
}

bool SessionRegistry::bind(ConnId conn, UserId user, const QString &displayName,
                           const QString &username)
{
    if (conn == 0 || user == 0)
        return false;

    bool becameOnline = false;
    {
        QWriteLocker locker(&m_lock);
        if (!m_workers.contains(conn))
            return false;

        if (const UserId old = m_connUser.value(conn, 0); old != 0 && old != user) {
            m_userConns.remove(old, conn);
            if (!m_userConns.contains(old))
                QMetaObject::invokeMethod(this, [this, old] { Q_EMIT userOffline(old); },
                                          Qt::QueuedConnection);
        }

        becameOnline = !m_userConns.contains(user);
        m_connUser.insert(conn, user);
        m_userConns.insert(user, conn);
        m_names.insert(user, displayName);
        if (!username.isEmpty())
            m_usernames.insert(user, username);
    }

    if (becameOnline)
        Q_EMIT userOnline(user);
    return true;
}

void SessionRegistry::unbind(ConnId conn)
{
    UserId user = 0;
    bool wentOffline = false;
    {
        QWriteLocker locker(&m_lock);
        user = m_connUser.take(conn);
        if (user == 0)
            return;
        m_userConns.remove(user, conn);
        wentOffline = !m_userConns.contains(user);
    }
    if (wentOffline)
        Q_EMIT userOffline(user);
}

UserId SessionRegistry::userOf(ConnId conn) const
{
    QReadLocker locker(&m_lock);
    return m_connUser.value(conn, 0);
}

QString SessionRegistry::displayName(UserId user) const
{
    QReadLocker locker(&m_lock);
    const QString name = m_names.value(user);
    return name.isEmpty() ? QStringLiteral("user-%1").arg(user) : name;
}

QString SessionRegistry::username(UserId user) const
{
    QReadLocker locker(&m_lock);
    return m_usernames.value(user);
}

QList<ConnId> SessionRegistry::connectionsOf(UserId user) const
{
    QReadLocker locker(&m_lock);
    return m_userConns.values(user);
}

bool SessionRegistry::isOnline(UserId user) const
{
    QReadLocker locker(&m_lock);
    return m_userConns.contains(user);
}

QList<UserId> SessionRegistry::onlineUsers() const
{
    QReadLocker locker(&m_lock);
    return m_userConns.uniqueKeys();
}

void SessionRegistry::sendToConn(ConnId conn, const proto::Envelope &envelope)
{
    NetWorker *worker = nullptr;
    {
        QReadLocker locker(&m_lock);
        worker = m_workers.value(conn, nullptr);
    }
    if (!worker)
        return;

    QMetaObject::invokeMethod(worker, "send", Qt::QueuedConnection,
                              Q_ARG(qm::ConnId, conn),
                              Q_ARG(qm::proto::Envelope, envelope));
}

void SessionRegistry::sendTo(UserId user, const proto::Envelope &envelope)
{
    const QList<ConnId> conns = connectionsOf(user);
    for (ConnId conn : conns)
        sendToConn(conn, envelope);
}

void SessionRegistry::broadcast(const QList<UserId> &users, const proto::Envelope &envelope)
{
    for (UserId user : users)
        sendTo(user, envelope);
}

void SessionRegistry::broadcastAll(const proto::Envelope &envelope)
{
    const QList<UserId> users = onlineUsers();
    broadcast(users, envelope);
}

} // namespace qm
