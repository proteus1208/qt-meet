#pragma once

#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QSslConfiguration>

#include <atomic>

#include "common/Ids.h"
#include "messages.pb.h"

namespace qm {

class Connection;

/// Aim: one network thread. Owns the Connections assigned to it by ConnectionManager and
/// forwards their envelopes to the Dispatcher; sends are queued into this thread.
class NetWorker : public QObject
{
    Q_OBJECT

public:
    explicit NetWorker(const QSslConfiguration &ssl, QObject *parent = nullptr);
    ~NetWorker() override;

    int connectionCount() const;

public Q_SLOTS:
    void adoptSocket(qintptr socketDescriptor, qm::ConnId id);
    void send(qm::ConnId id, const qm::proto::Envelope &envelope);
    void closeConnection(qm::ConnId id);
    void shutdown();

Q_SIGNALS:
    void envelopeReceived(qm::ConnId id, const qm::proto::Envelope &envelope);
    void connectionOpened(qm::ConnId id, const QHostAddress &peer);
    void connectionClosed(qm::ConnId id);

private:
    void onConnectionClosed(qm::ConnId id);

    QSslConfiguration          m_ssl;
    QHash<ConnId, Connection*> m_connections;
    std::atomic<int>           m_count{0};
};

} // namespace qm
