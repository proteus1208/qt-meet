#include "HttpMediaServer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QTcpServer>
#include <QUrl>
#include <QUrlQuery>

#ifdef QM_HAVE_HTTPSERVER
#include <QHttpHeaders>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#endif

#include "common/Ids.h"
#include "common/Logger.h"
#include "storage/StorageManager.h"

namespace qm {

HttpMediaServer::HttpMediaServer(StorageManager *storage, QObject *parent)
    : QObject(parent)
    , m_storage(storage)
{
}

HttpMediaServer::~HttpMediaServer()
{
    stop();
}

bool HttpMediaServer::start(const QHostAddress &address, quint16 port)
{
#ifdef QM_HAVE_HTTPSERVER
    if (m_http)
        return true;

    m_http = new QHttpServer(this);
    m_tcp = new QTcpServer(this);
    setupRoutes();

    if (!m_tcp->listen(address, port)) {
        qCWarning(lcApp) << "HTTP listen failed on" << port << m_tcp->errorString();
        stop();
        return false;
    }
    if (!m_http->bind(m_tcp)) {
        qCWarning(lcApp) << "HTTP bind failed";
        stop();
        return false;
    }

    const QString host = (address == QHostAddress::Any || address == QHostAddress::AnyIPv4)
                             ? QStringLiteral("127.0.0.1")
                             : address.toString();
    m_baseUrl = QStringLiteral("http://%1:%2").arg(host).arg(m_tcp->serverPort());
    qCInfo(lcApp) << "HTTP media server at" << m_baseUrl;
    return true;
#else
    Q_UNUSED(address)
    Q_UNUSED(port)
    qCWarning(lcApp) << "Qt HttpServer not available; /files downloads disabled";
    return false;
#endif
}

void HttpMediaServer::stop()
{
#ifdef QM_HAVE_HTTPSERVER
    if (m_tcp) {
        m_tcp->close();
        m_tcp->deleteLater();
        m_tcp = nullptr;
    }
    if (m_http) {
        m_http->deleteLater();
        m_http = nullptr;
    }
#endif
    m_baseUrl.clear();
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

void HttpMediaServer::setStorage(StorageManager *storage)
{
    m_storage = storage;
}

QString HttpMediaServer::baseUrl() const
{
    return m_baseUrl;
}

QString HttpMediaServer::issueToken(UserId user)
{
    const QString token = Ids::uuid();
    QWriteLocker locker(&m_lock);
    m_tokens.insert(token, user);
    return token;
}

void HttpMediaServer::revokeToken(const QString &token)
{
    QWriteLocker locker(&m_lock);
    m_tokens.remove(token);
}

void HttpMediaServer::revokeUser(UserId user)
{
    QWriteLocker locker(&m_lock);
    for (auto it = m_tokens.begin(); it != m_tokens.end();) {
        if (it.value() == user)
            it = m_tokens.erase(it);
        else
            ++it;
    }
}

UserId HttpMediaServer::validateToken(const QString &token) const
{
    if (token.isEmpty())
        return 0;
    QReadLocker locker(&m_lock);
    return m_tokens.value(token, 0);
}

void HttpMediaServer::setupRoutes()
{
#ifdef QM_HAVE_HTTPSERVER
    m_http->route(QStringLiteral("/files/<arg>"), QHttpServerRequest::Method::Get,
                  [this](const QString &fileId, const QHttpServerRequest &request) {
                      return serveBlob(fileId, request);
                  });

    // Nested paths via ?path=relative/file.mp4&t=<token> (arbitrary depth, sandbox-checked).
    m_http->route(QStringLiteral("/hosted"), QHttpServerRequest::Method::Get,
                  [this](const QHttpServerRequest &request) {
                      const QString rel = QUrlQuery(request.url()).queryItemValue(QStringLiteral("path"));
                      return serveHosted(rel, request);
                  });
#endif
}

#ifdef QM_HAVE_HTTPSERVER

UserId HttpMediaServer::tokenFromRequest(const QHttpServerRequest &request) const
{
    const QUrlQuery query(request.url());
    QString token = query.queryItemValue(QStringLiteral("t"));
    if (token.isEmpty()) {
        const QByteArray auth = request.value(QByteArrayLiteral("Authorization"));
        if (auth.startsWith("Bearer "))
            token = QString::fromUtf8(auth.mid(7));
    }
    return validateToken(token);
}

QHttpServerResponse HttpMediaServer::serveHosted(const QString &relativePath,
                                                 const QHttpServerRequest &request)
{
    if (tokenFromRequest(request) == 0)
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

    QString root;
    {
        QReadLocker locker(&m_lock);
        root = m_hostedRoot;
    }
    if (root.isEmpty())
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

    QString cleaned = relativePath;
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (cleaned.startsWith(QLatin1Char('/')))
        cleaned.remove(0, 1);
    if (cleaned.contains(QStringLiteral("..")))
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

    const QStringList parts = cleaned.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QLatin1String(".") || part == QLatin1String(".."))
            return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);
    }
    cleaned = parts.join(QLatin1Char('/'));
    if (cleaned.isEmpty())
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

    const QString rootAbs = QFileInfo(root).absoluteFilePath();
    const QString absolute = QDir(rootAbs).filePath(cleaned);
    const QFileInfo info(absolute);
    if (!info.exists() || !info.isFile())
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

    const QString cand = info.absoluteFilePath();
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    const QString prefix = rootAbs.endsWith(QLatin1Char('/')) ? rootAbs : rootAbs + QLatin1Char('/');
    if (!cand.startsWith(prefix, cs) && cand.compare(rootAbs, cs) != 0)
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

    return serveFileRange(cand, request);
}

QHttpServerResponse HttpMediaServer::serveBlob(const QString &fileId, const QHttpServerRequest &request)
{
    if (tokenFromRequest(request) == 0)
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
    if (!m_storage || fileId.isEmpty())
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

    const auto file = m_storage->fileById(fileId).result();
    if (!file || !file->complete || file->blobPath.isEmpty())
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

    return serveFileRange(file->blobPath, request);
}

QHttpServerResponse HttpMediaServer::serveFileRange(const QString &absolutePath,
                                                    const QHttpServerRequest &request)
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly))
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

    const qint64 size = file.size();
    const QString mime = QMimeDatabase().mimeTypeForFile(absolutePath).name();
    if (size <= 0) {
        return QHttpServerResponse(mime.toUtf8(), QByteArray(),
                                   QHttpServerResponse::StatusCode::Ok);
    }

    qint64 from = 0;
    qint64 to = size - 1;
    const QByteArray rangeHeader = request.value(QByteArrayLiteral("Range"));
    const bool partial = parseRange(rangeHeader, size, from, to);

    if (partial && (from < 0 || to < from || to >= size)) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::RequestRangeNotSatisfiable);
    }

    if (!file.seek(from))
        return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);

    const qint64 length = to - from + 1;
    const QByteArray data = file.read(length);

    QHttpHeaders headers;
    headers.append(QByteArrayLiteral("Accept-Ranges"), QByteArrayLiteral("bytes"));
    headers.append(QByteArrayLiteral("Content-Length"), QByteArray::number(data.size()));
    if (partial) {
        headers.append(QByteArrayLiteral("Content-Range"),
                       QByteArrayLiteral("bytes ") + QByteArray::number(from) + '-'
                           + QByteArray::number(to) + '/' + QByteArray::number(size));
    }

    return QHttpServerResponse(
        mime.toUtf8(), data, headers,
        partial ? QHttpServerResponse::StatusCode::PartialContent
                : QHttpServerResponse::StatusCode::Ok);
}

#endif // QM_HAVE_HTTPSERVER

bool HttpMediaServer::parseRange(const QByteArray &header, qint64 size, qint64 &from, qint64 &to)
{
    if (!header.startsWith("bytes=") || size <= 0)
        return false;

    const QByteArray spec = header.mid(6).trimmed();
    if (spec.contains(','))
        return false; // multi-range not supported

    const int dash = spec.indexOf('-');
    if (dash < 0)
        return false;

    const QByteArray start = spec.left(dash);
    const QByteArray end = spec.mid(dash + 1);

    if (start.isEmpty()) {
        // suffix: bytes=-N
        bool ok = false;
        const qint64 suffix = end.toLongLong(&ok);
        if (!ok || suffix <= 0)
            return false;
        from = qMax(qint64(0), size - suffix);
        to = size - 1;
        return true;
    }

    bool okFrom = false;
    from = start.toLongLong(&okFrom);
    if (!okFrom)
        return false;

    if (end.isEmpty()) {
        to = size - 1;
    } else {
        bool okTo = false;
        to = end.toLongLong(&okTo);
        if (!okTo)
            return false;
    }
    return true;
}

} // namespace qm
