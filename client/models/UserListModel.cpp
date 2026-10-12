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
    Q_UNUSED(index)
    Q_UNUSED(role)
    return {};
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
    Q_UNUSED(users)
}

void UserListModel::updatePresence(UserId user, proto::Presence presence)
{
    Q_UNUSED(user)
    Q_UNUSED(presence)
}

void UserListModel::clear()
{
}

QVariantMap UserListModel::get(int row) const
{
    Q_UNUSED(row)
    return {};
}

} // namespace qm
