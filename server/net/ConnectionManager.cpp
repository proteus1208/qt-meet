#include "ConnectionManager.h"

#include <QThread>

#include "NetWorker.h"
#include "SessionRegistry.h"

namespace qm {

ConnectionManager::ConnectionManager(int workerCount, const QSslConfiguration &ssl,
                                     SessionRegistry *sessions, QObject *parent)
    : QTcpServer(parent)
    , m_ssl(ssl)
    , m_sessions(sessions)
    , m_workerCount(workerCount)
{
}

ConnectionManager::~ConnectionManager()
{
    stop();
}

bool ConnectionManager::start(const QHostAddress &address, quint16 port)
{
    Q_UNUSED(address)
    Q_UNUSED(port)
    return false;
}

void ConnectionManager::stop()
{
}

int ConnectionManager::connectionCount() const
{
    return 0;
}

void ConnectionManager::incomingConnection(qintptr socketDescriptor)
{
    Q_UNUSED(socketDescriptor)
}

NetWorker *ConnectionManager::pickWorker()
{
    return nullptr;
}

} // namespace qm
