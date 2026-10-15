#include "Connection.h"

#include <QSslSocket>

#include "common/Logger.h"

namespace qm {

Connection::Connection(ConnId id, qintptr socketDescriptor, const QSslConfiguration &ssl, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_descriptor(socketDescriptor)
    , m_ssl(ssl)
{
}

Connection::~Connection()
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

bool Connection::start()
{
    m_socket = new QSslSocket(this);
    m_socket->setSslConfiguration(m_ssl);
    m_socket->setPeerVerifyMode(QSslSocket::VerifyNone);

    if (!m_socket->setSocketDescriptor(m_descriptor)) {
        qCWarning(lcNet) << "Connection" << m_id << "setSocketDescriptor failed";
        return false;
    }

    connect(m_socket, &QSslSocket::readyRead, this, &Connection::onReadyRead);
    connect(m_socket, &QSslSocket::disconnected, this, &Connection::onDisconnected);
    connect(m_socket, &QSslSocket::sslErrors, this, &Connection::onSslErrors);
    connect(m_socket, &QSslSocket::encrypted, this, [this] {
        qCInfo(lcNet) << "Connection" << m_id << "TLS ready from" << peerAddress();
        m_lastActivity.start();
    });

    m_socket->startServerEncryption();
    m_lastActivity.start();
    return true;
}

ConnId Connection::id() const
{
    return m_id;
}

QHostAddress Connection::peerAddress() const
{
    return m_socket ? m_socket->peerAddress() : QHostAddress{};
}

qint64 Connection::idleMs() const
{
    return m_lastActivity.isValid() ? m_lastActivity.elapsed() : 0;
}

void Connection::send(const proto::Envelope &envelope)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    const QByteArray frame = FrameCodec::encode(envelope);
    if (frame.isEmpty())
        return;
    m_socket->write(frame);
    m_lastActivity.restart();
}

void Connection::close()
{
    if (m_socket)
        m_socket->disconnectFromHost();
}

void Connection::onReadyRead()
{
    if (!m_socket)
        return;

    const QByteArray chunk = m_socket->readAll();
    m_lastActivity.restart();
    if (!m_codec.feed(chunk)) {
        qCWarning(lcNet) << "Connection" << m_id << "corrupt frame; closing";
        close();
        return;
    }

    proto::Envelope env;
    while (m_codec.next(env)) {
        Q_EMIT envelopeReceived(m_id, env);
        env.Clear();
    }
}

void Connection::onDisconnected()
{
    qCInfo(lcNet) << "Connection" << m_id << "closed";
    Q_EMIT closed(m_id);
}

void Connection::onSslErrors(const QList<QSslError> &errors)
{
    for (const QSslError &err : errors)
        qCWarning(lcNet) << "Connection" << m_id << "SSL:" << err.errorString();
    // Dev / offline LAN: accept self-signed peer on server side is N/A; server presents cert.
    m_socket->ignoreSslErrors();
}

} // namespace qm
