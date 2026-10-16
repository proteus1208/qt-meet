#pragma once

#include <QFuture>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPromise>
#include <QThread>

#include <atomic>
#include <memory>

#include "common/FrameCodec.h"

namespace qm {

class NetworkWorker;

/// Aim: the client's single TLS control connection to the server, running on its own thread.
/// Correlates replies to requests by seq, pushes server events.
class NetworkClient : public QObject
{
    Q_OBJECT

public:
    enum class State { Disconnected, Connecting, Connected, Reconnecting };
    Q_ENUM(State)

    explicit NetworkClient(QObject *parent = nullptr);
    ~NetworkClient() override;

    void start();
    void shutdown();

    void connectToServer(const QString &host, quint16 port, const QString &caFile = {});
    void disconnectFromServer();

    QFuture<proto::Envelope> request(proto::Envelope envelope, int timeoutMs = 10000);
    void                     post(proto::Envelope envelope);

    State state() const;

Q_SIGNALS:
    void envelopeReceived(const qm::proto::Envelope &envelope);
    void stateChanged(qm::NetworkClient::State state);
    void errorOccurred(const QString &message);

private:

    void setState(State state);
    void onWorkerFrame(const qm::proto::Envelope &envelope);
    void failPending(const QString &reason);

    QThread  m_thread;
    NetworkWorker *m_worker = nullptr;
    QString  m_host;
    quint16  m_port = 7400;
    QString  m_caFile;
    std::atomic<State> m_state{State::Disconnected};

    QMutex                                                     m_pendingMutex;
    QHash<quint64, std::shared_ptr<QPromise<proto::Envelope>>> m_pending;
};

} // namespace qm
