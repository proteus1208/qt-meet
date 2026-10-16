#include "SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QSharedMemory>

namespace qm {

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent)
    , m_key(key)
{
}

SingleInstance::~SingleInstance()
{
    if (m_server) {
        m_server->close();
        QLocalServer::removeServer(m_key);
    }
    if (m_memory && m_memory->isAttached())
        m_memory->detach();
}

bool SingleInstance::tryBecomePrimary()
{
    m_memory = new QSharedMemory(m_key + QStringLiteral("-lock"), this);
    if (m_memory->attach()) {
        // Stale lock from a crashed process — clear and retry once.
        m_memory->detach();
    }
    if (!m_memory->create(1)) {
        if (m_memory->error() == QSharedMemory::AlreadyExists)
            return false;
        // Fall through and still try the local server.
    }

    QLocalServer::removeServer(m_key);
    m_server = new QLocalServer(this);
    if (!m_server->listen(m_key))
        return false;

    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (m_server->hasPendingConnections()) {
            QLocalSocket *sock = m_server->nextPendingConnection();
            if (!sock)
                continue;
            connect(sock, &QLocalSocket::readyRead, this, [this, sock] {
                const QByteArray msg = sock->readAll();
                sock->disconnectFromServer();
                sock->deleteLater();
                if (msg.contains("raise") || msg.isEmpty())
                    Q_EMIT activationRequested();
            });
            connect(sock, &QLocalSocket::disconnected, sock, &QObject::deleteLater);
        }
    });
    return true;
}

bool SingleInstance::notifyPrimary(const QString &key, const QByteArray &message)
{
    QLocalSocket sock;
    sock.connectToServer(key);
    if (!sock.waitForConnected(500))
        return false;
    sock.write(message.isEmpty() ? QByteArrayLiteral("raise") : message);
    sock.flush();
    sock.waitForBytesWritten(500);
    sock.disconnectFromServer();
    return true;
}

} // namespace qm
