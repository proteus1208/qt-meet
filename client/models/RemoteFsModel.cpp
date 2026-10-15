#include "RemoteFsModel.h"

#include <QUrlQuery>

namespace qm {

namespace {

bool mimeIsPlayable(const QString &mime)
{
    return mime.startsWith(QStringLiteral("video/")) || mime.startsWith(QStringLiteral("audio/"));
}

QString joinPath(const QString &dir, const QString &name)
{
    QString base = dir;
    if (base.isEmpty() || base == QLatin1String("/"))
        return QLatin1Char('/') + name;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    return base + QLatin1Char('/') + name;
}

} // namespace

RemoteFsModel::RemoteFsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int RemoteFsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant RemoteFsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const proto::DirEntry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return QString::fromStdString(entry.name());
    case IsDirRole:
        return entry.is_dir();
    case SizeRole:
        return qint64(entry.size());
    case ModifiedRole:
        return qint64(entry.modified_ms());
    case MimeRole:
        return QString::fromStdString(entry.mime());
    case IsPlayableRole:
        return !entry.is_dir() && mimeIsPlayable(QString::fromStdString(entry.mime()));
    default:
        return {};
    }
}

QHash<int, QByteArray> RemoteFsModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {IsDirRole, "isDir"},
        {SizeRole, "size"},
        {ModifiedRole, "modified"},
        {MimeRole, "mime"},
        {IsPlayableRole, "isPlayable"},
    };
}

void RemoteFsModel::setListing(const proto::DirListRes &listing, const QString &token)
{
    const QString newPath = QString::fromStdString(listing.path());
    beginResetModel();
    m_entries.clear();
    m_entries.reserve(listing.entries_size());
    for (const proto::DirEntry &entry : listing.entries())
        m_entries.append(entry);
    m_path = newPath.isEmpty() ? QStringLiteral("/") : newPath;
    m_urlPrefix = QString::fromStdString(listing.http_url_prefix());
    m_token = token;
    endResetModel();
    Q_EMIT pathChanged();
}

void RemoteFsModel::clear()
{
    if (m_entries.isEmpty() && m_path.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    m_path.clear();
    m_urlPrefix.clear();
    m_token.clear();
    endResetModel();
    Q_EMIT pathChanged();
}

QString RemoteFsModel::path() const
{
    return m_path;
}

QUrl RemoteFsModel::urlOf(int row) const
{
    if (row < 0 || row >= m_entries.size() || m_urlPrefix.isEmpty() || m_token.isEmpty())
        return {};
    const proto::DirEntry &entry = m_entries.at(row);
    if (entry.is_dir())
        return {};

    QString rel = joinPath(m_path, QString::fromStdString(entry.name()));
    while (rel.startsWith(QLatin1Char('/')))
        rel.remove(0, 1);

    QUrl url(m_urlPrefix);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("path"), rel);
    query.addQueryItem(QStringLiteral("t"), m_token);
    url.setQuery(query);
    return url;
}

bool RemoteFsModel::isDir(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return false;
    return m_entries.at(row).is_dir();
}

QString RemoteFsModel::nameOf(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};
    return QString::fromStdString(m_entries.at(row).name());
}

} // namespace qm
