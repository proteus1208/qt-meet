#include "FolderService.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>

#include "common/EnvelopeFactory.h"
#include "common/Logger.h"
#include "net/SessionRegistry.h"
#include "streaming/HttpMediaServer.h"

namespace qm {

namespace {

bool isUnderRoot(const QString &rootAbs, const QString &candidateAbs)
{
    const QString root = QFileInfo(rootAbs).absoluteFilePath();
    const QString cand = QFileInfo(candidateAbs).absoluteFilePath();
    if (root.isEmpty() || cand.isEmpty())
        return false;
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    if (cand.compare(root, cs) == 0)
        return true;
    const QString prefix = root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/');
    return cand.startsWith(prefix, cs);
}

/// Returns false if the path escapes the sandbox (.., absolute drive, etc.).
bool sanitizeRelative(const QString &path, QString &relativeOut)
{
    QString cleaned = path;
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (cleaned.contains(QStringLiteral("..")))
        return false;

    while (cleaned.startsWith(QLatin1Char('/')))
        cleaned.remove(0, 1);
    while (cleaned.endsWith(QLatin1Char('/')) && !cleaned.isEmpty())
        cleaned.chop(1);

    const QStringList parts = cleaned.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QLatin1String(".") || part == QLatin1String(".."))
            return false;
        if (part.contains(QLatin1Char(':'))) // Windows drive fragment
            return false;
    }
    relativeOut = parts.join(QLatin1Char('/'));
    return true;
}

} // namespace

FolderService::FolderService(SessionRegistry *sessions, HttpMediaServer *http)
    : m_sessions(sessions)
    , m_http(http)
{
}

void FolderService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    if (envelope.body_case() == proto::Envelope::kDirListReq)
        onDirList(context, envelope);
}

void FolderService::setRoot(const QString &absolutePath)
{
    QWriteLocker locker(&m_lock);
    m_root = absolutePath;
    if (m_http)
        m_http->setHostedRoot(absolutePath);
}

QString FolderService::root() const
{
    QReadLocker locker(&m_lock);
    return m_root;
}

bool FolderService::resolve(const QString &relativePath, QString &absolutePath) const
{
    QReadLocker locker(&m_lock);
    if (m_root.isEmpty())
        return false;

    QString rel;
    if (!sanitizeRelative(relativePath, rel))
        return false;

    const QString rootAbs = QFileInfo(m_root).absoluteFilePath();
    if (!QFileInfo(rootAbs).isDir())
        return false;

    const QString candidate = rel.isEmpty() ? rootAbs : QDir(rootAbs).filePath(rel);
    const QFileInfo info(candidate);
    if (!info.exists())
        return false;
    if (!isUnderRoot(rootAbs, info.absoluteFilePath()))
        return false;
    absolutePath = info.absoluteFilePath();
    return true;
}

void FolderService::onDirList(const RequestContext &context, const proto::Envelope &envelope)
{
    const QString requested = QString::fromStdString(envelope.dir_list_req().path());
    QString rel;
    if (!sanitizeRelative(requested, rel)) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_FORBIDDEN,
                                                      QStringLiteral("Invalid path")));
        return;
    }

    QString absolute;
    if (!resolve(rel, absolute)) {
        const QString root = this->root();
        m_sessions->sendToConn(
            context.conn,
            EnvelopeFactory::error(envelope, proto::ERR_NOT_FOUND,
                                   root.isEmpty() ? QStringLiteral("No hosted folder configured")
                                                  : QStringLiteral("Folder not found")));
        return;
    }

    QFileInfo info(absolute);
    if (!info.isDir()) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INVALID,
                                                      QStringLiteral("Not a directory")));
        return;
    }

    proto::Envelope reply = EnvelopeFactory::reply(envelope);
    auto *res = reply.mutable_dir_list_res();
    const QString pathOut = rel.isEmpty() ? QStringLiteral("/") : (QLatin1Char('/') + rel);
    res->set_path(pathOut.toStdString());

    QString prefix = m_http ? m_http->baseUrl() : QString();
    if (prefix.endsWith(QLatin1Char('/')))
        prefix.chop(1);
    if (!prefix.isEmpty())
        prefix += QStringLiteral("/hosted");
    res->set_http_url_prefix(prefix.toStdString());

    QMimeDatabase mimeDb;
    QDir dir(absolute);
    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                           QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &entry : entries) {
        auto *e = res->add_entries();
        e->set_name(entry.fileName().toStdString());
        e->set_is_dir(entry.isDir());
        e->set_size(entry.isDir() ? 0 : quint64(entry.size()));
        e->set_modified_ms(entry.lastModified().toMSecsSinceEpoch());
        if (!entry.isDir())
            e->set_mime(mimeDb.mimeTypeForFile(entry).name().toStdString());
    }

    qCInfo(lcApp) << "DirList" << pathOut << "entries" << res->entries_size();
    m_sessions->sendToConn(context.conn, reply);
}

} // namespace qm
