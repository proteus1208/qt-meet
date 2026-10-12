#pragma once

#include <QFuture>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPromise>
#include <QSslError>
#include <QThread>

#include <atomic>
#include <memory>

#include "common/FrameCodec.h"

class QSslSocket;
class QTimer;

namespace qm {

/// Aim: the client's single TLS control connection to the server, running on its own thread.
/// Correlates replies to requests by seq, pushes server events, reconnects automatically.
class NetworkClient : public QObject
{
    Q_OBJECT

public:
    enum class State { Disconnected, Connecting, Connected, Reconnecting };
    Q_ENUM(State)

    explicit NetworkClient(QObject *parent = nullptr);
    ~NetworkClient() override;

    void start();                 // moves the socket side into the network thread
    void shutdown();

    void connectToServer(const QString &host, quint16 port, const QString &caFile = {});
    void disconnectFromServer();

    QFuture<proto::Envelope> request(proto::Envelope envelope, int timeoutMs = 10000);
    void                     post(proto::Envelope envelope);      // fire and forget

    State state() const;

Q_SIGNALS:
    void event(const qm::proto::Envelope &envelope);    // server-initiated (reply_to == 0)
    void stateChanged(qm::NetworkClient::State state);
    void errorOccurred(const QString &message);

private:
    void doConnect();
    void doSend(const proto::Envelope &envelope);
    void onReadyRead();
    void onEncrypted();
    void onDisconnected();
    void onSslErrors(const QList<QSslError> &errors);
    void failPending(const QString &reason);
    void setState(State state);

    QThread           m_thread;
    QSslSocket       *m_socket = nullptr;
    QTimer           *m_reconnectTimer = nullptr;
    FrameCodec        m_codec;
    QString           m_host;
    quint16           m_port = 0;
    QString           m_caFile;
    std::atomic<State> m_state{State::Disconnected};

    QMutex                                                     m_pendingMutex;
    QHash<quint64, std::shared_ptr<QPromise<proto::Envelope>>> m_pending;
};

} // namespace qm
