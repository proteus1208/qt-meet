#pragma once

#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <QUrlQuery>

#include "common/Ids.h"

class QTcpServer;
class QTcpSocket;

namespace qm {

class StorageManager;

/// Aim: HTTP endpoint for media bytes, with Range support so players can seek:
///   GET /hosted?path=<relative>&t=<token>   files from the hosted folder
///   GET /files/<fileId>?t=<token>           chat attachments from the blob store
/// Tokens are issued at login (AuthService) and checked on every request.
///
/// Implemented with QTcpServer (no Qt HttpServer module).
class HttpMediaServer : public QObject
{
    Q_OBJECT

public:
    explicit HttpMediaServer(StorageManager *storage = nullptr, QObject *parent = nullptr);
    ~HttpMediaServer() override;

    bool start(const QHostAddress &address, quint16 port);
    void stop();
    bool isListening() const;

    void    setHostedRoot(const QString &absolutePath);
    QString hostedRoot() const;
    void    setBlobDir(const QString &absolutePath);
    void    setStorage(StorageManager *storage);
    QString baseUrl() const; // http://<server-ip>:<port>, empty if not listening

    QString issueToken(UserId user);
    void    revokeToken(const QString &token);
    void    revokeUser(UserId user);
    UserId  validateToken(const QString &token) const;

private slots:
    void onNewConnection();

private:
    struct HttpRequest {
        QString method;
        QString path;
        QUrlQuery query;
        QHash<QByteArray, QByteArray> headers; // lower-case keys
    };

    void handleClient(QTcpSocket *socket);
    bool readRequest(QTcpSocket *socket, HttpRequest *out);
    void writeResponse(QTcpSocket *socket, int status, const QByteArray &reason,
                       const QByteArray &contentType, const QByteArray &body,
                       const QList<QPair<QByteArray, QByteArray>> &extraHeaders = {});
    void writeStatus(QTcpSocket *socket, int status, const QByteArray &reason);

    void serveHosted(QTcpSocket *socket, const QString &relativePath, const HttpRequest &request);
    void serveBlob(QTcpSocket *socket, const QString &fileId, const HttpRequest &request);
    void serveFileRange(QTcpSocket *socket, const QString &absolutePath, const HttpRequest &request);
    UserId tokenFromRequest(const HttpRequest &request) const;

    static bool parseRange(const QByteArray &header, qint64 size, qint64 &from, qint64 &to);

    QTcpServer             *m_tcp = nullptr;
    StorageManager         *m_storage = nullptr;
    mutable QReadWriteLock  m_lock;
    QString                 m_hostedRoot;
    QString                 m_blobDir;
    QHash<QString, UserId>  m_tokens;
    QString                 m_baseUrl;
};

} // namespace qm
