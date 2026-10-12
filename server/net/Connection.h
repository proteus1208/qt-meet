#pragma once

#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>
#include <QSslConfiguration>
#include <QSslError>

#include "common/FrameCodec.h"
#include "common/Ids.h"

class QSslSocket;

namespace qm {

/// Aim: one client TLS connection. Lives entirely in its NetWorker thread; turns bytes into
/// envelopes (FrameCodec) and serializes outgoing envelopes.
class Connection : public QObject
{
    Q_OBJECT

public:
    Connection(ConnId id, qintptr socketDescriptor, const QSslConfiguration &ssl, QObject *parent = nullptr);
    ~Connection() override;

    bool         start();
    ConnId       id() const;
    QHostAddress peerAddress() const;
    qint64       idleMs() const;

public Q_SLOTS:
    void send(const qm::proto::Envelope &envelope);
    void close();

Q_SIGNALS:
    void envelopeReceived(qm::ConnId id, const qm::proto::Envelope &envelope);
    void closed(qm::ConnId id);

private:
    void onReadyRead();
    void onDisconnected();
    void onSslErrors(const QList<QSslError> &errors);

    ConnId            m_id;
    qintptr           m_descriptor;
    QSslConfiguration m_ssl;
    QSslSocket       *m_socket = nullptr;
    FrameCodec        m_codec;
    QElapsedTimer     m_lastActivity;
};

} // namespace qm
