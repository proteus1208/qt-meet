#pragma once

#include <QObject>

#include "common/FrameCodec.h"

class QSslSocket;

namespace qm {

/// Aim: owns the QSslSocket on the network thread for NetworkClient; frames in, envelopes out.
class NetworkWorker : public QObject
{
    Q_OBJECT

public:
    explicit NetworkWorker(QObject *parent = nullptr);

public Q_SLOTS:
    void connectTo(const QString &host, quint16 port);
    void disconnectHost();
    void sendFrame(const QByteArray &frame);

Q_SIGNALS:
    void encrypted();
    void disconnected();
    void error(const QString &message);
    void envelope(const qm::proto::Envelope &env);

private:
    void onReadyRead();

    QSslSocket *m_socket = nullptr;
    FrameCodec  m_codec;
};

} // namespace qm
