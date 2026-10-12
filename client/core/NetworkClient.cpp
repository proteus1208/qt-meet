#include "NetworkClient.h"

#include <QSslSocket>
#include <QTimer>

namespace qm {

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent)
{
}

NetworkClient::~NetworkClient()
{
    shutdown();
}

void NetworkClient::start()
{
}

void NetworkClient::shutdown()
{
    m_thread.quit();
    m_thread.wait();
}

void NetworkClient::connectToServer(const QString &host, quint16 port, const QString &caFile)
{
    m_host = host;
    m_port = port;
    m_caFile = caFile;
}

void NetworkClient::disconnectFromServer()
{
}

QFuture<proto::Envelope> NetworkClient::request(proto::Envelope envelope, int timeoutMs)
{
    Q_UNUSED(envelope)
    Q_UNUSED(timeoutMs)
    return {};
}

void NetworkClient::post(proto::Envelope envelope)
{
    Q_UNUSED(envelope)
}

NetworkClient::State NetworkClient::state() const
{
    return m_state.load();
}

void NetworkClient::doConnect()
{
}

void NetworkClient::doSend(const proto::Envelope &envelope)
{
    Q_UNUSED(envelope)
}

void NetworkClient::onReadyRead()
{
}

void NetworkClient::onEncrypted()
{
}

void NetworkClient::onDisconnected()
{
}

void NetworkClient::onSslErrors(const QList<QSslError> &errors)
{
    Q_UNUSED(errors)
}

void NetworkClient::failPending(const QString &reason)
{
    Q_UNUSED(reason)
}

void NetworkClient::setState(State state)
{
    Q_UNUSED(state)
}

} // namespace qm
