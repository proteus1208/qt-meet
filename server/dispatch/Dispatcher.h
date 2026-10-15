#pragma once

#include <QHash>
#include <QHostAddress>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QThreadPool>

#include "IHandler.h"

namespace qm {

class SessionRegistry;

/// Aim: route each incoming envelope by its body case to the registered handler and run it on
/// the handler thread pool. Envelopes of one connection run in arrival order (per-connection
/// queue); different connections run in parallel.
class Dispatcher : public QObject
{
    Q_OBJECT

public:
    Dispatcher(SessionRegistry *sessions, int threadCount, QObject *parent = nullptr);
    ~Dispatcher() override;

    void registerHandler(proto::Envelope::BodyCase bodyCase, IHandler *handler);
    void shutdown();

public Q_SLOTS:
    void dispatch(qm::ConnId conn, const qm::proto::Envelope &envelope);
    void onConnectionOpened(qm::ConnId conn, const QHostAddress &peer);
    void onConnectionClosed(qm::ConnId conn);

private:
    struct ConnQueue
    {
        QQueue<proto::Envelope> pending;
        QHostAddress            peer;
        bool                    running = false;
    };

    void drain(ConnId conn);
    void rejectUnauthenticated(const RequestContext &context, const proto::Envelope &envelope);

    SessionRegistry           *m_sessions = nullptr;
    QThreadPool                m_pool;
    QHash<int, IHandler *>     m_handlers;
    QMutex                     m_queuesMutex;
    QHash<ConnId, ConnQueue>   m_queues;
};

} // namespace qm
