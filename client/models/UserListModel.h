#pragma once

#include <QAbstractListModel>
#include <QList>

#include "common/Ids.h"
#include "messages.pb.h"

namespace qm {

/// Aim: users for QML (search results / contacts) with live presence.
class UserListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role { UserIdRole = Qt::UserRole + 1, UsernameRole, DisplayNameRole, PresenceRole };

    explicit UserListModel(QObject *parent = nullptr);

    int                    rowCount(const QModelIndex &parent = {}) const override;
    QVariant               data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setUsers(const QList<proto::UserInfo> &users);
    void updatePresence(UserId user, proto::Presence presence);
    void clear();

    Q_INVOKABLE QVariantMap get(int row) const;

private:
    QList<proto::UserInfo> m_users;
};

} // namespace qm
