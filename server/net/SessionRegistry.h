#pragma once

#include <QHash>
#include <QList>
#include <QMultiHash>
#include <QObject>
#include <QReadWriteLock>

#include "common/Ids.h"
#include "messages.pb.h"

namespace qm {

class NetWorker;

/// Aim: thread-safe directory of who is online: ConnId -> owning NetWorker, ConnId <-> UserId.
/// The only way services send to clients (sendTo / sendToConn queue into the owning thread).
class SessionRegistry : public QObject
{
    Q_OBJECT

public:
    explicit SessionRegistry(QObject *parent = nullptr);
    ~SessionRegistry() override;

    void registerConnection(ConnId conn, NetWorker *worker);
    void unregisterConnection(ConnId conn);

    bool bind(ConnId conn, UserId user, const QString &displayName);   // after successful login
    void unbind(ConnId conn);

    UserId        userOf(ConnId conn) const;
    QString       displayName(UserId user) const;
    QList<ConnId> connectionsOf(UserId user) const;
    bool          isOnline(UserId user) const;
    QList<UserId> onlineUsers() const;

    void sendToConn(ConnId conn, const proto::Envelope &envelope);
    void sendTo(UserId user, const proto::Envelope &envelope);
    void broadcast(const QList<UserId> &users, const proto::Envelope &envelope);
    void broadcastAll(const proto::Envelope &envelope);

Q_SIGNALS:
    void userOnline(qm::UserId user);
    void userOffline(qm::UserId user);

private:
    mutable QReadWriteLock      m_lock;
    QHash<ConnId, NetWorker *>  m_workers;
    QHash<ConnId, UserId>       m_connUser;
    QMultiHash<UserId, ConnId>  m_userConns;
    QHash<UserId, QString>      m_names;
};

} // namespace qm
