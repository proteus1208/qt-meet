#include "HttpMediaServer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

#include "common/Ids.h"
#include "common/Logger.h"
#include "storage/StorageManager.h"

namespace qm {

namespace {

constexpr int kHeaderLimit = 64 * 1024;

QByteArray statusReason(int status)
{
    switch (status) {
    case 200: return QByteArrayLiteral("OK");
    case 206: return QByteArrayLiteral("Partial Content");
    case 400: return QByteArrayLiteral("Bad Request");
    case 401: return QByteArrayLiteral("Unauthorized");
    case 403: return QByteArrayLiteral("Forbidden");
    case 404: return QByteArrayLiteral("Not Found");
    case 405: return QByteArrayLiteral("Method Not Allowed");
    case 416: return QByteArrayLiteral("Range Not Satisfiable");
    case 500: return QByteArrayLiteral("Internal Server Error");
    default:  return QByteArrayLiteral("Error");
    }
}

} // namespace

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
    if (m_tcp)
        return true;

    m_tcp = new QTcpServer(this);
    connect(m_tcp, &QTcpServer::newConnection, this, &HttpMediaServer::onNewConnection);

    if (!m_tcp->listen(address, port)) {
        qCWarning(lcApp) << "HTTP listen failed on" << port << m_tcp->errorString();
        stop();
        return false;
    }

    const QString host = (address == QHostAddress::Any || address == QHostAddress::AnyIPv4)
                             ? QStringLiteral("127.0.0.1")
                             : address.toString();
    m_baseUrl = QStringLiteral("http://%1:%2").arg(host).arg(m_tcp->serverPort());
    qCInfo(lcApp) << "HTTP media server at" << m_baseUrl;
    return true;
}

void HttpMediaServer::stop()
{
    if (m_tcp) {
        m_tcp->close();
        m_tcp->deleteLater();
        m_tcp = nullptr;
    }
    m_baseUrl.clear();
}

bool HttpMediaServer::isListening() const
{
    return m_tcp && m_tcp->isListening() && !m_baseUrl.isEmpty();
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

void HttpMediaServer::onNewConnection()
{
    while (m_tcp && m_tcp->hasPendingConnections()) {
        QTcpSocket *socket = m_tcp->nextPendingConnection();
        if (!socket)
            continue;
        socket->setParent(this);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            handleClient(socket);
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        if (socket->bytesAvailable() > 0)
            handleClient(socket);
    }
}

void HttpMediaServer::handleClient(QTcpSocket *socket)
{
    if (!socket || socket->property("qm_handled").toBool())
        return;

    HttpRequest req;
    if (!readRequest(socket, &req)) {
        if (socket->property("qm_bad").toBool()) {
            writeStatus(socket, 400, statusReason(400));
            socket->disconnectFromHost();
        }
        return; // wait for more bytes
    }
    socket->setProperty("qm_handled", true);

    if (req.method != QLatin1String("GET") && req.method != QLatin1String("HEAD")) {
        writeStatus(socket, 405, statusReason(405));
        socket->disconnectFromHost();
        return;
    }

    if (req.path == QLatin1String("/hosted")) {
        serveHosted(socket, req.query.queryItemValue(QStringLiteral("path")), req);
    } else if (req.path.startsWith(QLatin1String("/files/"))) {
        const QString fileId = req.path.mid(QStringLiteral("/files/").size());
        serveBlob(socket, QUrl::fromPercentEncoding(fileId.toUtf8()), req);
    } else {
        writeStatus(socket, 404, statusReason(404));
    }
    socket->disconnectFromHost();
}

bool HttpMediaServer::readRequest(QTcpSocket *socket, HttpRequest *out)
{
    const QByteArray buf = socket->peek(kHeaderLimit + 4);
    const int sep = buf.indexOf("\r\n\r\n");
    if (sep < 0) {
        if (buf.size() >= kHeaderLimit)
            socket->setProperty("qm_bad", true);
        return false;
    }

    const QByteArray headerBlock = socket->read(sep + 4);
    const QList<QByteArray> lines = headerBlock.split('\n');
    if (lines.isEmpty()) {
        socket->setProperty("qm_bad", true);
        return false;
    }

    QByteArray requestLine = lines.first().trimmed();
    if (requestLine.endsWith('\r'))
        requestLine.chop(1);
    const QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 2) {
        socket->setProperty("qm_bad", true);
        return false;
    }

    out->method = QString::fromLatin1(parts[0]).toUpper();
    const QByteArray target = parts[1];
    const int qmark = target.indexOf('?');
    const QByteArray pathBytes = qmark >= 0 ? target.left(qmark) : target;
    const QByteArray queryBytes = qmark >= 0 ? target.mid(qmark + 1) : QByteArray();
    out->path = QString::fromUtf8(QByteArray::fromPercentEncoding(pathBytes));
    out->query = QUrlQuery(QString::fromUtf8(queryBytes));
    out->headers.clear();

    for (int i = 1; i < lines.size(); ++i) {
        QByteArray line = lines[i];
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.isEmpty())
            break;
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QByteArray key = line.left(colon).trimmed().toLower();
        const QByteArray value = line.mid(colon + 1).trimmed();
        out->headers.insert(key, value);
    }
    return true;
}

void HttpMediaServer::writeStatus(QTcpSocket *socket, int status, const QByteArray &reason)
{
    writeResponse(socket, status, reason, QByteArrayLiteral("text/plain"), reason);
}

void HttpMediaServer::writeResponse(QTcpSocket *socket, int status, const QByteArray &reason,
                                    const QByteArray &contentType, const QByteArray &body,
                                    const QList<QPair<QByteArray, QByteArray>> &extraHeaders)
{
    if (!socket)
        return;
    QByteArray msg;
    msg += "HTTP/1.1 ";
    msg += QByteArray::number(status);
    msg += ' ';
    msg += reason;
    msg += "\r\n";
    msg += "Connection: close\r\n";
    msg += "Content-Type: ";
    msg += contentType.isEmpty() ? QByteArrayLiteral("application/octet-stream") : contentType;
    msg += "\r\n";
    msg += "Content-Length: ";
    msg += QByteArray::number(body.size());
    msg += "\r\n";
    for (const auto &h : extraHeaders) {
        msg += h.first;
        msg += ": ";
        msg += h.second;
        msg += "\r\n";
    }
    msg += "\r\n";
    socket->write(msg);
    if (!body.isEmpty())
        socket->write(body);
    socket->flush();
}

UserId HttpMediaServer::tokenFromRequest(const HttpRequest &request) const
{
    QString token = request.query.queryItemValue(QStringLiteral("t"));
    if (token.isEmpty()) {
        const QByteArray auth = request.headers.value(QByteArrayLiteral("authorization"));
        if (auth.startsWith("Bearer "))
            token = QString::fromUtf8(auth.mid(7));
    }
    return validateToken(token);
}

void HttpMediaServer::serveHosted(QTcpSocket *socket, const QString &relativePath,
                                  const HttpRequest &request)
{
    if (tokenFromRequest(request) == 0) {
        writeStatus(socket, 401, statusReason(401));
        return;
    }

    QString root;
    {
        QReadLocker locker(&m_lock);
        root = m_hostedRoot;
    }
    if (root.isEmpty()) {
        writeStatus(socket, 404, statusReason(404));
        return;
    }

    QString cleaned = relativePath;
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (cleaned.startsWith(QLatin1Char('/')))
        cleaned.remove(0, 1);
    if (cleaned.contains(QStringLiteral(".."))) {
        writeStatus(socket, 403, statusReason(403));
        return;
    }

    const QStringList parts = cleaned.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QLatin1String(".") || part == QLatin1String("..")) {
            writeStatus(socket, 403, statusReason(403));
            return;
        }
    }
    cleaned = parts.join(QLatin1Char('/'));
    if (cleaned.isEmpty()) {
        writeStatus(socket, 404, statusReason(404));
        return;
    }

    const QString rootAbs = QFileInfo(root).absoluteFilePath();
    const QString absolute = QDir(rootAbs).filePath(cleaned);
    const QFileInfo info(absolute);
    if (!info.exists() || !info.isFile()) {
        writeStatus(socket, 404, statusReason(404));
        return;
    }

    const QString cand = info.absoluteFilePath();
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    const QString prefix = rootAbs.endsWith(QLatin1Char('/')) ? rootAbs : rootAbs + QLatin1Char('/');
    if (!cand.startsWith(prefix, cs) && cand.compare(rootAbs, cs) != 0) {
        writeStatus(socket, 403, statusReason(403));
        return;
    }

    serveFileRange(socket, cand, request);
}

void HttpMediaServer::serveBlob(QTcpSocket *socket, const QString &fileId, const HttpRequest &request)
{
    if (tokenFromRequest(request) == 0) {
        writeStatus(socket, 401, statusReason(401));
        return;
    }
    if (!m_storage || fileId.isEmpty()) {
        writeStatus(socket, 404, statusReason(404));
        return;
    }

    const auto file = m_storage->fileById(fileId).result();
    if (!file || !file->complete || file->blobPath.isEmpty()) {
        writeStatus(socket, 404, statusReason(404));
        return;
    }

    serveFileRange(socket, file->blobPath, request);
}

void HttpMediaServer::serveFileRange(QTcpSocket *socket, const QString &absolutePath,
                                     const HttpRequest &request)
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        writeStatus(socket, 404, statusReason(404));
        return;
    }

    const qint64 size = file.size();
    const QByteArray mime = QMimeDatabase().mimeTypeForFile(absolutePath).name().toUtf8();
    if (size <= 0) {
        writeResponse(socket, 200, statusReason(200), mime, QByteArray());
        return;
    }

    qint64 from = 0;
    qint64 to = size - 1;
    const QByteArray rangeHeader = request.headers.value(QByteArrayLiteral("range"));
    const bool partial = parseRange(rangeHeader, size, from, to);

    if (partial && (from < 0 || to < from || to >= size)) {
        QList<QPair<QByteArray, QByteArray>> headers;
        headers.append({QByteArrayLiteral("Content-Range"),
                        QByteArrayLiteral("bytes */") + QByteArray::number(size)});
        writeResponse(socket, 416, statusReason(416), QByteArrayLiteral("text/plain"),
                      statusReason(416), headers);
        return;
    }

    if (!file.seek(from)) {
        writeStatus(socket, 500, statusReason(500));
        return;
    }

    const qint64 length = to - from + 1;
    const QByteArray data = (request.method == QLatin1String("HEAD"))
                                ? QByteArray()
                                : file.read(length);

    QList<QPair<QByteArray, QByteArray>> headers;
    headers.append({QByteArrayLiteral("Accept-Ranges"), QByteArrayLiteral("bytes")});
    if (partial) {
        headers.append({QByteArrayLiteral("Content-Range"),
                        QByteArrayLiteral("bytes ") + QByteArray::number(from) + '-'
                            + QByteArray::number(to) + '/' + QByteArray::number(size)});
    }

    // For HEAD, advertise the real body length even though we send no body.
    QByteArray msg;
    const int status = partial ? 206 : 200;
    msg += "HTTP/1.1 ";
    msg += QByteArray::number(status);
    msg += ' ';
    msg += statusReason(status);
    msg += "\r\n";
    msg += "Connection: close\r\n";
    msg += "Content-Type: ";
    msg += mime.isEmpty() ? QByteArrayLiteral("application/octet-stream") : mime;
    msg += "\r\n";
    msg += "Content-Length: ";
    msg += QByteArray::number(request.method == QLatin1String("HEAD") ? length : data.size());
    msg += "\r\n";
    for (const auto &h : headers) {
        msg += h.first;
        msg += ": ";
        msg += h.second;
        msg += "\r\n";
    }
    msg += "\r\n";
    socket->write(msg);
    if (!data.isEmpty())
        socket->write(data);
    socket->flush();
}

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
