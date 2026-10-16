#include "ExplorerController.h"

#include <QFutureWatcher>
#include <QStandardPaths>
#include <QUrl>

#include "common/EnvelopeFactory.h"
#include "common/Logger.h"
#include "controllers/SessionController.h"
#include "core/FileTransfer.h"
#include "core/NetworkClient.h"
#include "models/RemoteFsModel.h"

namespace qm {

ExplorerController::ExplorerController(NetworkClient *network, SessionController *session,
                                       FileTransfer *transfers, RemoteFsModel *files,
                                       QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_session(session)
    , m_transfers(transfers)
    , m_files(files)
{
    if (m_transfers) {
        connect(m_transfers, &FileTransfer::downloaded, this,
                [this](const QString &id, const QString &localPath) {
                    if (!m_pendingDownloads.remove(id))
                        return;
                    Q_EMIT downloadFinished(localPath);
                });
        connect(m_transfers, &FileTransfer::failed, this,
                [this](const QString &id, const QString &reason) {
                    if (!id.isEmpty() && !m_pendingDownloads.remove(id))
                        return;
                    Q_EMIT errorOccurred(reason);
                });
    }

    connect(m_session, &SessionController::sessionChanged, this, [this] {
        if (!m_session->loggedIn()) {
            m_latestListSeq = 0;
            m_files->clear();
            m_currentPath.clear();
            clearMedia();
            Q_EMIT currentPathChanged();
            return;
        }
        openPath(QStringLiteral("/"));
    });
}

QAbstractItemModel *ExplorerController::files() const
{
    return m_files;
}

QString ExplorerController::currentPath() const
{
    return m_currentPath.isEmpty() ? QStringLiteral("/") : m_currentPath;
}

bool ExplorerController::canGoUp() const
{
    const QString path = currentPath();
    return !path.isEmpty() && path != QLatin1String("/");
}

bool ExplorerController::busy() const
{
    return m_busy;
}

QUrl ExplorerController::mediaUrl() const
{
    return m_mediaUrl;
}

void ExplorerController::refresh()
{
    openPath(currentPath());
}

void ExplorerController::openPath(const QString &path)
{
    QString normalized = path.trimmed();
    if (normalized.isEmpty())
        normalized = QStringLiteral("/");
    if (!normalized.startsWith(QLatin1Char('/')))
        normalized.prepend(QLatin1Char('/'));
    while (normalized.size() > 1 && normalized.endsWith(QLatin1Char('/')))
        normalized.chop(1);

    m_currentPath = normalized;
    Q_EMIT currentPathChanged();
    requestListing(normalized);
}

void ExplorerController::requestListing(const QString &path)
{
    if (!m_session->loggedIn())
        return;

    proto::Envelope env = EnvelopeFactory::request();
    auto *req = env.mutable_dir_list_req();
    req->set_path(path.toStdString());

    const quint64 seq = env.seq();
    m_latestListSeq = seq;
    m_busy = true;
    Q_EMIT busyChanged();

    auto *watcher = new QFutureWatcher<proto::Envelope>(this);
    connect(watcher, &QFutureWatcher<proto::Envelope>::finished, this, [this, watcher, seq] {
        watcher->deleteLater();
        if (seq == m_latestListSeq) {
            m_busy = false;
            Q_EMIT busyChanged();
        }
        if (seq != m_latestListSeq)
            return;

        proto::Envelope reply;
        try {
            reply = watcher->result();
        } catch (const std::exception &ex) {
            qCWarning(lcNet) << "DirList failed:" << ex.what();
            Q_EMIT errorOccurred(QString::fromUtf8(ex.what()));
            return;
        }
        if (reply.has_error()) {
            Q_EMIT errorOccurred(QString::fromStdString(reply.error().message()));
            return;
        }
        if (!reply.has_dir_list_res())
            return;
        onListing(reply.dir_list_res());
    });
    watcher->setFuture(m_network->request(env, 15000));
}

void ExplorerController::openEntry(int row)
{
    if (!m_files || row < 0 || row >= m_files->rowCount())
        return;

    if (m_files->isDir(row)) {
        const QString name = m_files->nameOf(row);
        const QString base = currentPath();
        const QString next = (base == QLatin1String("/"))
                                 ? (QLatin1Char('/') + name)
                                 : (base + QLatin1Char('/') + name);
        openPath(next);
        return;
    }

    const QModelIndex idx = m_files->index(row, 0);
    const bool playable = idx.isValid()
                          && m_files->data(idx, RemoteFsModel::IsPlayableRole).toBool();
    if (!playable) {
        // Non-media files: download instead of attempting playback.
        downloadEntry(row);
        return;
    }

    const QUrl url = m_files->urlOf(row);
    if (url.isEmpty()) {
        Q_EMIT errorOccurred(
            QStringLiteral("Cannot play: HTTP media server is not available on the "
                           "QtMeet server (port 7401). Check the admin window for "
                           "\"HTTP … (DOWN)\" and install Qt HTTP Server if needed."));
        return;
    }
    openMedia(url);
}

void ExplorerController::downloadEntry(int row)
{
    if (!m_files || !m_transfers || row < 0 || row >= m_files->rowCount())
        return;
    if (m_files->isDir(row)) {
        Q_EMIT errorOccurred(QStringLiteral("Cannot download a folder"));
        return;
    }

    const QUrl url = m_files->urlOf(row);
    if (url.isEmpty()) {
        Q_EMIT errorOccurred(
            QStringLiteral("Cannot download: HTTP media server is not available "
                           "(port 7401)."));
        return;
    }

    const QString name = m_files->nameOf(row);
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QString transferId = m_transfers->downloadUrl(url, name, dir);
    if (transferId.isEmpty())
        return;
    m_pendingDownloads.insert(transferId);
    Q_EMIT downloadStarted(name);
}

void ExplorerController::goUp()
{
    if (!canGoUp())
        return;
    QString path = currentPath();
    while (path.endsWith(QLatin1Char('/')) && path.size() > 1)
        path.chop(1);
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    openPath(slash <= 0 ? QStringLiteral("/") : path.left(slash));
}

void ExplorerController::openMedia(const QUrl &url)
{
    if (url.isEmpty())
        return;
    if (m_mediaUrl == url) {
        // Force PlayerPage to reload the same clip if opened again.
        m_mediaUrl.clear();
        Q_EMIT mediaUrlChanged();
    }
    m_mediaUrl = url;
    Q_EMIT mediaUrlChanged();
    Q_EMIT playRequested(url.fileName().isEmpty() ? url.toString() : url.fileName());
}

void ExplorerController::clearMedia()
{
    if (m_mediaUrl.isEmpty())
        return;
    m_mediaUrl.clear();
    Q_EMIT mediaUrlChanged();
}

void ExplorerController::onListing(const proto::DirListRes &listing)
{
    proto::DirListRes adjusted = listing;
    const QString rawPrefix = QString::fromStdString(adjusted.http_url_prefix());
    QUrl prefix(rawPrefix);
    const QString host = prefix.host();
    const bool loopback = host == QLatin1String("127.0.0.1") || host == QLatin1String("localhost")
                          || host == QLatin1String("::1");
    if (!rawPrefix.isEmpty() && (host.isEmpty() || loopback)) {
        // Prefer login http base (already rewritten), else the TCP host we connected to.
        QUrl fromSession(m_session->httpBaseUrl());
        if (prefix.scheme().isEmpty())
            prefix.setScheme(fromSession.scheme().isEmpty() ? QStringLiteral("http")
                                                            : fromSession.scheme());
        prefix.setHost(fromSession.host().isEmpty() ? m_session->serverHost() : fromSession.host());
        if (prefix.port() <= 0)
            prefix.setPort(fromSession.port() > 0 ? fromSession.port() : 7401);
        if (prefix.path().isEmpty())
            prefix.setPath(QStringLiteral("/hosted"));
        adjusted.set_http_url_prefix(prefix.toString(QUrl::RemoveQuery).toStdString());
    } else if (rawPrefix.isEmpty()) {
        // Server HTTP media is down — keep empty so openEntry reports a clear error.
        adjusted.clear_http_url_prefix();
    }

    m_files->setListing(adjusted, m_session->sessionToken());
    const QString listed = QString::fromStdString(adjusted.path());
    if (!listed.isEmpty() && listed != m_currentPath) {
        m_currentPath = listed;
        Q_EMIT currentPathChanged();
    }
}

} // namespace qm
