#pragma once

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QString>

#include <optional>

#include "common/Ids.h"
#include "storage/StorageTypes.h"

namespace qm {

/// Phase 1 user directory (replaced by StorageManager SQLite in Phase 2).
class InMemoryUsers
{
public:
    static InMemoryUsers &instance()
    {
        static InMemoryUsers store;
        return store;
    }

    std::optional<UserRecord> findByName(const QString &username) const
    {
        QMutexLocker locker(&m_mutex);
        const auto it = m_byName.constFind(username.toLower());
        if (it == m_byName.cend())
            return std::nullopt;
        return *it;
    }

    std::optional<UserRecord> findById(UserId id) const
    {
        QMutexLocker locker(&m_mutex);
        const auto it = m_byId.constFind(id);
        if (it == m_byId.cend())
            return std::nullopt;
        return *it;
    }

    UserRecord create(const QString &username, const QString &displayName,
                      const QByteArray &hash, const QByteArray &salt)
    {
        QMutexLocker locker(&m_mutex);
        UserRecord user;
        user.id = ++m_nextId;
        user.username = username;
        user.displayName = displayName;
        user.passwordHash = hash;
        user.salt = salt;
        user.createdMs = QDateTime::currentMSecsSinceEpoch();
        m_byName.insert(username.toLower(), user);
        m_byId.insert(user.id, user);
        return user;
    }

    QList<UserRecord> search(const QString &query, int limit, UserId exclude) const
    {
        QMutexLocker locker(&m_mutex);
        QList<UserRecord> out;
        const QString q = query.trimmed().toLower();
        for (const UserRecord &user : m_byId) {
            if (user.id == exclude)
                continue;
            if (!q.isEmpty()
                && !user.username.toLower().contains(q)
                && !user.displayName.toLower().contains(q)) {
                continue;
            }
            out.append(user);
            if (out.size() >= limit)
                break;
        }
        return out;
    }

    QString displayName(UserId id) const
    {
        const auto user = findById(id);
        return user ? user->displayName : QStringLiteral("user-%1").arg(id);
    }

private:
    InMemoryUsers() = default;

    mutable QMutex m_mutex;
    QHash<QString, UserRecord> m_byName;
    QHash<UserId, UserRecord> m_byId;
    UserId m_nextId = 0;
};

} // namespace qm
