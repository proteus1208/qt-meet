#pragma once

#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QReadWriteLock>
#include <QString>

#include "common/Ids.h"

class QHttpServer;
class QHttpServerRequest;
class QHttpServerResponse;
class QTcpServer;

namespace qm {

class StorageManager;

/// Aim: HTTP endpoint for media bytes, with Range support so players can seek:
///   GET /hosted/<relative path>?t=<token>   files from the hosted folder
///   GET /files/<fileId>?t=<token>           chat attachments from the blob store
/// Tokens are issued at login (AuthService) and checked on every request.
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
    QString baseUrl() const;                         // http://<server-ip>:<port>, empty if not listening

    QString issueToken(UserId user);
    void    revokeToken(const QString &token);
    void    revokeUser(UserId user);
    UserId  validateToken(const QString &token) const;

private:
    void setupRoutes();
#ifdef QM_HAVE_HTTPSERVER
    QHttpServerResponse serveHosted(const QString &relativePath, const QHttpServerRequest &request);
    QHttpServerResponse serveBlob(const QString &fileId, const QHttpServerRequest &request);
    QHttpServerResponse serveFileRange(const QString &absolutePath, const QHttpServerRequest &request);
    UserId tokenFromRequest(const QHttpServerRequest &request) const;
#endif

    static bool parseRange(const QByteArray &header, qint64 size, qint64 &from, qint64 &to);

    QHttpServer            *m_http = nullptr;
    QTcpServer             *m_tcp = nullptr;
    StorageManager         *m_storage = nullptr;
    mutable QReadWriteLock  m_lock;
    QString                 m_hostedRoot;
    QString                 m_blobDir;
    QHash<QString, UserId>  m_tokens;
    QString                 m_baseUrl;
};

} // namespace qm
