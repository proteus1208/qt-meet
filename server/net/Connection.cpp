#include "Connection.h"

#include <QSslSocket>

namespace qm {

Connection::Connection(ConnId id, qintptr socketDescriptor, const QSslConfiguration &ssl, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_descriptor(socketDescriptor)
    , m_ssl(ssl)
{
}

Connection::~Connection() = default;

bool Connection::start()
{
    return false;
}

ConnId Connection::id() const
{
    return m_id;
}

QHostAddress Connection::peerAddress() const
{
    return {};
}

qint64 Connection::idleMs() const
{
    return m_lastActivity.isValid() ? m_lastActivity.elapsed() : 0;
}

void Connection::send(const proto::Envelope &envelope)
{
    Q_UNUSED(envelope)
}

void Connection::close()
{
}

void Connection::onReadyRead()
{
}

void Connection::onDisconnected()
{
}

void Connection::onSslErrors(const QList<QSslError> &errors)
{
    Q_UNUSED(errors)
}

} // namespace qm
