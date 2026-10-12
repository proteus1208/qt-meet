#include "RemoteFsModel.h"

namespace qm {

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
    Q_UNUSED(index)
    Q_UNUSED(role)
    return {};
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
    Q_UNUSED(listing)
    m_token = token;
}

void RemoteFsModel::clear()
{
}

QString RemoteFsModel::path() const
{
    return m_path;
}

QUrl RemoteFsModel::urlOf(int row) const
{
    Q_UNUSED(row)
    return {};
}

bool RemoteFsModel::isDir(int row) const
{
    Q_UNUSED(row)
    return false;
}

QString RemoteFsModel::nameOf(int row) const
{
    Q_UNUSED(row)
    return {};
}

} // namespace qm
