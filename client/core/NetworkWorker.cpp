#include "NetworkWorker.h"

#include <QSslConfiguration>
#include <QSslSocket>

#include "common/Logger.h"

namespace qm {

NetworkWorker::NetworkWorker(QObject *parent)
    : QObject(parent)
{
}

void NetworkWorker::connectTo(const QString &host, quint16 port)
{
    if (!m_socket) {
        m_socket = new QSslSocket(this);
        connect(m_socket, &QSslSocket::encrypted, this, &NetworkWorker::encrypted);
        connect(m_socket, &QSslSocket::readyRead, this, &NetworkWorker::onReadyRead);
        connect(m_socket, &QSslSocket::disconnected, this, &NetworkWorker::disconnected);
        connect(m_socket, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
            Q_EMIT error(m_socket->errorString());
        });
        connect(m_socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
            for (const QSslError &err : errors)
                qCWarning(lcNet) << "Client SSL:" << err.errorString();
            m_socket->ignoreSslErrors();
        });
    }

    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->abort();

    m_codec.reset();
    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);
    m_socket->setSslConfiguration(conf);
    qCInfo(lcNet) << "Connecting to" << host << port;
    m_socket->connectToHostEncrypted(host, port);
}

void NetworkWorker::disconnectHost()
{
    if (m_socket)
        m_socket->disconnectFromHost();
}

void NetworkWorker::sendFrame(const QByteArray &frame)
{
    if (m_socket && m_socket->isEncrypted() && !frame.isEmpty())
        m_socket->write(frame);
}

void NetworkWorker::onReadyRead()
{
    if (!m_codec.feed(m_socket->readAll())) {
        Q_EMIT error(QStringLiteral("Corrupt framing from server"));
        m_socket->abort();
        return;
    }
    proto::Envelope env;
    while (m_codec.next(env)) {
        Q_EMIT envelope(env);
        env.Clear();
    }
}

} // namespace qm
