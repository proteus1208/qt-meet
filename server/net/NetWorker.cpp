#include "NetWorker.h"

#include "Connection.h"
#include "common/Ids.h"
#include "common/Logger.h"

namespace qm {

NetWorker::NetWorker(const QSslConfiguration &ssl, QObject *parent)
    : QObject(parent)
    , m_ssl(ssl)
{
}

NetWorker::~NetWorker()
{
    shutdown();
}

int NetWorker::connectionCount() const
{
    return m_count.load();
}

void NetWorker::adoptSocket(qintptr socketDescriptor, ConnId id)
{
    auto *conn = new Connection(id, socketDescriptor, m_ssl, this);
    if (!conn->start()) {
        delete conn;
        return;
    }

    m_connections.insert(id, conn);
    m_count.store(m_connections.size());

    connect(conn, &Connection::envelopeReceived, this, &NetWorker::envelopeReceived);
    connect(conn, &Connection::closed, this, &NetWorker::onConnectionClosed);

    Q_EMIT connectionOpened(id, conn->peerAddress());
    qCInfo(lcNet) << "NetWorker adopted conn" << id << "count" << m_count.load();
}

void NetWorker::send(ConnId id, const proto::Envelope &envelope)
{
    if (auto *conn = m_connections.value(id, nullptr))
        conn->send(envelope);
}

void NetWorker::closeConnection(ConnId id)
{
    if (auto *conn = m_connections.value(id, nullptr))
        conn->close();
}

void NetWorker::shutdown()
{
    const auto ids = m_connections.keys();
    for (ConnId id : ids)
        closeConnection(id);
    qDeleteAll(m_connections);
    m_connections.clear();
    m_count.store(0);
}

void NetWorker::onConnectionClosed(ConnId id)
{
    if (auto *conn = m_connections.take(id)) {
        conn->deleteLater();
        m_count.store(m_connections.size());
        Q_EMIT connectionClosed(id);
    }
}

} // namespace qm
