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

/// Aim: HTTP endpoint for media bytes, with Range support so players can seek:
///   GET /hosted/<relative path>?t=<token>   files from the hosted folder
///   GET /files/<fileId>?t=<token>           chat attachments from the blob store
/// Tokens are issued at login (AuthService) and checked on every request.
class HttpMediaServer : public QObject
{
    Q_OBJECT

public:
    explicit HttpMediaServer(QObject *parent = nullptr);
    ~HttpMediaServer() override;

    bool start(const QHostAddress &address, quint16 port);
    void stop();

    void    setHostedRoot(const QString &absolutePath);
    QString hostedRoot() const;
    void    setBlobDir(const QString &absolutePath);
    QString baseUrl() const;                         // http://<server-ip>:<port>

    QString issueToken(UserId user);
    void    revokeToken(const QString &token);
    UserId  validateToken(const QString &token) const;

private:
    void setupRoutes();
    QHttpServerResponse serveHosted(const QString &relativePath, const QHttpServerRequest &request);
    QHttpServerResponse serveBlob(const QString &fileId, const QHttpServerRequest &request);
    QHttpServerResponse serveFileRange(const QString &absolutePath, const QHttpServerRequest &request);

    static bool parseRange(const QByteArray &header, qint64 size, qint64 &from, qint64 &to);

    QHttpServer            *m_http = nullptr;
    QTcpServer             *m_tcp = nullptr;
    mutable QReadWriteLock  m_lock;
    QString                 m_hostedRoot;
    QString                 m_blobDir;
    QHash<QString, UserId>  m_tokens;
    QString                 m_baseUrl;
};

} // namespace qm
