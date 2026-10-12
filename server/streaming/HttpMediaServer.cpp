#include "HttpMediaServer.h"

#include <QTcpServer>

#ifdef QM_HAVE_HTTPSERVER
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#endif

namespace qm {

HttpMediaServer::HttpMediaServer(QObject *parent)
    : QObject(parent)
{
}

HttpMediaServer::~HttpMediaServer()
{
    stop();
}

bool HttpMediaServer::start(const QHostAddress &address, quint16 port)
{
    Q_UNUSED(address)
    Q_UNUSED(port)
    return false;
}

void HttpMediaServer::stop()
{
}

void HttpMediaServer::setHostedRoot(const QString &absolutePath)
{
    QWriteLocker locker(&m_lock);
    m_hostedRoot = absolutePath;
}

QString HttpMediaServer::hostedRoot() const
{
    QReadLocker locker(&m_lock);
    return m_hostedRoot;
}

void HttpMediaServer::setBlobDir(const QString &absolutePath)
{
    QWriteLocker locker(&m_lock);
    m_blobDir = absolutePath;
}

QString HttpMediaServer::baseUrl() const
{
    return m_baseUrl;
}

QString HttpMediaServer::issueToken(UserId user)
{
    Q_UNUSED(user)
    return {};
}

void HttpMediaServer::revokeToken(const QString &token)
{
    Q_UNUSED(token)
}

UserId HttpMediaServer::validateToken(const QString &token) const
{
    Q_UNUSED(token)
    return 0;
}

void HttpMediaServer::setupRoutes()
{
}

#ifdef QM_HAVE_HTTPSERVER
QHttpServerResponse HttpMediaServer::serveHosted(const QString &relativePath, const QHttpServerRequest &request)
{
    Q_UNUSED(relativePath)
    Q_UNUSED(request)
    return QHttpServerResponse(QHttpServerResponse::StatusCode::NotImplemented);
}

QHttpServerResponse HttpMediaServer::serveBlob(const QString &fileId, const QHttpServerRequest &request)
{
    Q_UNUSED(fileId)
    Q_UNUSED(request)
    return QHttpServerResponse(QHttpServerResponse::StatusCode::NotImplemented);
}

QHttpServerResponse HttpMediaServer::serveFileRange(const QString &absolutePath, const QHttpServerRequest &request)
{
    Q_UNUSED(absolutePath)
    Q_UNUSED(request)
    return QHttpServerResponse(QHttpServerResponse::StatusCode::NotImplemented);
}
#endif

bool HttpMediaServer::parseRange(const QByteArray &header, qint64 size, qint64 &from, qint64 &to)
{
    Q_UNUSED(header)
    Q_UNUSED(size)
    Q_UNUSED(from)
    Q_UNUSED(to)
    return false;
}

} // namespace qm
