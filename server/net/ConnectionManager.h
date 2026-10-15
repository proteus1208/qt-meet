#pragma once

#include <QHostAddress>
#include <QList>
#include <QSslConfiguration>
#include <QTcpServer>

#include "common/Ids.h"
#include "messages.pb.h"

class QThread;

namespace qm {

class NetWorker;
class SessionRegistry;

/// Aim: accept TCP connections and distribute socket descriptors round-robin (least loaded)
/// over N NetWorker threads. Never touches sockets itself.
class ConnectionManager : public QTcpServer
{
    Q_OBJECT

public:
    ConnectionManager(int workerCount, const QSslConfiguration &ssl,
                      SessionRegistry *sessions, QObject *parent = nullptr);
    ~ConnectionManager() override;

    bool start(const QHostAddress &address, quint16 port);
    void stop();
    int  connectionCount() const;

Q_SIGNALS:
    void envelopeReceived(qm::ConnId id, const qm::proto::Envelope &envelope);
    void connectionOpened(qm::ConnId id, const QHostAddress &peer);
    void connectionClosed(qm::ConnId id);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    NetWorker *pickWorker();

    QSslConfiguration  m_ssl;
    SessionRegistry   *m_sessions = nullptr;
    QList<NetWorker *> m_workers;
    QList<QThread *>   m_threads;
    int                m_workerCount = 0;
    int                m_next = 0;
};

} // namespace qm
