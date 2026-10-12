#include "UserListModel.h"

namespace qm {

UserListModel::UserListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int UserListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_users.size());
}

QVariant UserListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_users.size())
        return {};

    const proto::UserInfo &user = m_users.at(index.row());
    switch (role) {
    case UserIdRole:
        return qint64(user.id());
    case UsernameRole:
        return QString::fromStdString(user.username());
    case Qt::DisplayRole:
    case DisplayNameRole:
        return QString::fromStdString(user.display_name());
    case PresenceRole:
        return int(user.presence());
    default:
        return {};
    }
}

QHash<int, QByteArray> UserListModel::roleNames() const
{
    return {
        {UserIdRole, "userId"},
        {UsernameRole, "username"},
        {DisplayNameRole, "displayName"},
        {PresenceRole, "presence"},
    };
}

void UserListModel::setUsers(const QList<proto::UserInfo> &users)
{
    beginResetModel();
    m_users = users;
    endResetModel();
}

bool UserListModel::updatePresence(UserId user, proto::Presence presence)
{
    for (int row = 0; row < m_users.size(); ++row) {
        if (m_users[row].id() != user)
            continue;
        if (m_users[row].presence() != presence) {
            m_users[row].set_presence(presence);
            const QModelIndex idx = index(row);
            Q_EMIT dataChanged(idx, idx, {PresenceRole});
        }
        return true;
    }
    return false;
}

proto::Presence UserListModel::presenceOf(UserId user) const
{
    for (const proto::UserInfo &info : m_users) {
        if (info.id() == user)
            return info.presence();
    }
    return proto::PRESENCE_OFFLINE;
}

void UserListModel::clear()
{
    if (m_users.isEmpty())
        return;
    beginResetModel();
    m_users.clear();
    endResetModel();
}

QVariantMap UserListModel::get(int row) const
{
    if (row < 0 || row >= m_users.size())
        return {};
    const proto::UserInfo &user = m_users.at(row);
    return {
        {QStringLiteral("userId"), qint64(user.id())},
        {QStringLiteral("username"), QString::fromStdString(user.username())},
        {QStringLiteral("displayName"), QString::fromStdString(user.display_name())},
        {QStringLiteral("presence"), int(user.presence())},
    };
}

} // namespace qm
