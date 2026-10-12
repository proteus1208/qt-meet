#include "NetWorker.h"

#include "Connection.h"

namespace qm {

NetWorker::NetWorker(const QSslConfiguration &ssl, QObject *parent)
    : QObject(parent)
    , m_ssl(ssl)
{
}

NetWorker::~NetWorker() = default;

int NetWorker::connectionCount() const
{
    return m_count.load();
}

void NetWorker::adoptSocket(qintptr socketDescriptor, ConnId id)
{
    Q_UNUSED(socketDescriptor)
    Q_UNUSED(id)
}

void NetWorker::send(ConnId id, const proto::Envelope &envelope)
{
    Q_UNUSED(id)
    Q_UNUSED(envelope)
}

void NetWorker::closeConnection(ConnId id)
{
    Q_UNUSED(id)
}

void NetWorker::shutdown()
{
}

void NetWorker::onConnectionClosed(ConnId id)
{
    Q_UNUSED(id)
}

} // namespace qm
