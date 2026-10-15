#include "StorageManager.h"

#include <QDateTime>
#include <QPromise>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <atomic>
#include <memory>

#include "common/Logger.h"

namespace qm {

namespace {

constexpr int kSchemaVersion = 1;

// SQLite extended result code SQLITE_CONSTRAINT_UNIQUE.
const QString kUniqueViolation = QStringLiteral("2067");

QString nextConnectionName()
{
    static std::atomic<int> counter{0};
    return QStringLiteral("qm-storage-%1").arg(++counter);
}

QString likePattern(const QString &query)
{
    QString escaped = query.trimmed();
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('%'), QStringLiteral("\\%"));
    escaped.replace(QLatin1Char('_'), QStringLiteral("\\_"));
    return QLatin1Char('%') + escaped + QLatin1Char('%');
}

UserRecord readUser(const QSqlQuery &q)
{
    UserRecord user;
    user.id = q.value(0).toULongLong();
    user.username = q.value(1).toString();
    user.displayName = q.value(2).toString();
    user.passwordHash = q.value(3).toByteArray();
    user.salt = q.value(4).toByteArray();
    user.createdMs = q.value(5).toLongLong();
    return user;
}

constexpr auto kUserColumns = "id, username, display_name, password_hash, salt, created_ms";

} // namespace

StorageManager::StorageManager(const QString &databasePath, QObject *parent)
    : QObject(parent)
    , m_databasePath(databasePath)
    , m_connectionName(nextConnectionName())
{
}

StorageManager::~StorageManager()
{
    stop();
}

template <typename T>
QFuture<T> StorageManager::post(std::function<T()> job)
{
    auto promise = std::make_shared<QPromise<T>>();
    QFuture<T> future = promise->future();
    promise->start();

    if (!m_worker || !m_thread.isRunning()) {
        qCWarning(lcStorage) << "Storage job submitted while storage is stopped";
        promise->addResult(T{});
        promise->finish();
        return future;
    }

    QMetaObject::invokeMethod(m_worker, [promise, job = std::move(job)] {
        promise->addResult(job());
        promise->finish();
    }, Qt::QueuedConnection);
    return future;
}

QFuture<bool> StorageManager::start()
{
    if (m_thread.isRunning())
        return QtFuture::makeReadyValueFuture(true);

    m_worker = new QObject;
    m_worker->moveToThread(&m_thread);
    m_thread.setObjectName(QStringLiteral("StorageThread"));
    m_thread.start();

    return post<bool>([this] { return openDatabase() && migrate(); });
}

void StorageManager::stop()
{
    if (!m_worker)
        return;

    if (m_thread.isRunning()) {
        QMetaObject::invokeMethod(m_worker, [this] { closeDatabase(); },
                                  Qt::BlockingQueuedConnection);
        m_thread.quit();
        m_thread.wait();
    }
    delete m_worker;
    m_worker = nullptr;
}

QFuture<std::optional<UserRecord>> StorageManager::userByName(const QString &username)
{
    return post<std::optional<UserRecord>>([this, username]() -> std::optional<UserRecord> {
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral("SELECT %1 FROM users WHERE username = ?").arg(kUserColumns));
        q.addBindValue(username);
        if (!q.exec()) {
            qCWarning(lcStorage) << "userByName failed:" << q.lastError().text();
            return std::nullopt;
        }
        if (!q.next())
            return std::nullopt;
        return readUser(q);
    });
}

QFuture<std::optional<UserRecord>> StorageManager::userById(UserId id)
{
    return post<std::optional<UserRecord>>([this, id]() -> std::optional<UserRecord> {
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral("SELECT %1 FROM users WHERE id = ?").arg(kUserColumns));
        q.addBindValue(qint64(id));
        if (!q.exec()) {
            qCWarning(lcStorage) << "userById failed:" << q.lastError().text();
            return std::nullopt;
        }
        if (!q.next())
            return std::nullopt;
        return readUser(q);
    });
}

QFuture<std::optional<UserRecord>> StorageManager::createUser(const UserRecord &user)
{
    return post<std::optional<UserRecord>>([this, user]() -> std::optional<UserRecord> {
        UserRecord created = user;
        if (created.createdMs == 0)
            created.createdMs = QDateTime::currentMSecsSinceEpoch();

        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral(
            "INSERT INTO users (username, display_name, password_hash, salt, created_ms) "
            "VALUES (?, ?, ?, ?, ?)"));
        q.addBindValue(created.username);
        q.addBindValue(created.displayName);
        q.addBindValue(created.passwordHash);
        q.addBindValue(created.salt);
        q.addBindValue(created.createdMs);
        if (!q.exec()) {
            if (q.lastError().nativeErrorCode() != kUniqueViolation)
                qCWarning(lcStorage) << "createUser failed:" << q.lastError().text();
            return std::nullopt;
        }
        created.id = q.lastInsertId().toULongLong();
        return created;
    });
}

QFuture<QList<UserRecord>> StorageManager::searchUsers(const QString &query, int limit, UserId excludeId)
{
    return post<QList<UserRecord>>([this, query, limit, excludeId] {
        QList<UserRecord> out;
        const QString pattern = likePattern(query);

        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral(
            R"(SELECT %1 FROM users
               WHERE (username LIKE ? ESCAPE '\' OR display_name LIKE ? ESCAPE '\')
                 AND id <> ?
               ORDER BY display_name COLLATE NOCASE
               LIMIT ?)").arg(kUserColumns));
        q.addBindValue(pattern);
        q.addBindValue(pattern);
        q.addBindValue(qint64(excludeId));
        q.addBindValue(limit);
        if (!q.exec()) {
            qCWarning(lcStorage) << "searchUsers failed:" << q.lastError().text();
            return out;
        }
        while (q.next())
            out.append(readUser(q));
        return out;
    });
}

QFuture<ConversationId> StorageManager::directConversation(UserId a, UserId b)
{
    return post<ConversationId>([this, a, b]() -> ConversationId {
        if (a == 0 || b == 0 || a == b)
            return 0;

        const UserId lo = qMin(a, b);
        const UserId hi = qMax(a, b);
        const QString key = QStringLiteral("%1:%2").arg(lo).arg(hi);
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);

        {
            QSqlQuery q(db);
            q.prepare(QStringLiteral("SELECT id FROM conversations WHERE direct_key = ?"));
            q.addBindValue(key);
            if (!q.exec()) {
                qCWarning(lcStorage) << "directConversation lookup failed:" << q.lastError().text();
                return 0;
            }
            if (q.next())
                return q.value(0).toULongLong();
        }

        if (!db.transaction()) {
            qCWarning(lcStorage) << "directConversation begin failed:" << db.lastError().text();
            return 0;
        }

        ConversationId id = 0;
        {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "INSERT INTO conversations (kind, direct_key, created_ms) VALUES (?, ?, ?)"));
            q.addBindValue(int(ConversationRecord::Kind::Direct));
            q.addBindValue(key);
            q.addBindValue(QDateTime::currentMSecsSinceEpoch());
            if (!q.exec()) {
                // Concurrent create: look up again.
                db.rollback();
                QSqlQuery again(db);
                again.prepare(QStringLiteral("SELECT id FROM conversations WHERE direct_key = ?"));
                again.addBindValue(key);
                if (again.exec() && again.next())
                    return again.value(0).toULongLong();
                qCWarning(lcStorage) << "directConversation insert failed:" << q.lastError().text();
                return 0;
            }
            id = q.lastInsertId().toULongLong();
        }

        for (UserId member : {lo, hi}) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "INSERT INTO conversation_members (conversation_id, user_id) VALUES (?, ?)"));
            q.addBindValue(qint64(id));
            q.addBindValue(qint64(member));
            if (!q.exec()) {
                qCWarning(lcStorage) << "directConversation member failed:" << q.lastError().text();
                db.rollback();
                return 0;
            }
        }

        if (!db.commit()) {
            qCWarning(lcStorage) << "directConversation commit failed:" << db.lastError().text();
            return 0;
        }
        return id;
    });
}

QFuture<ConversationId> StorageManager::createMeetingConversation(const QList<UserId> &members)
{
    return post<ConversationId>([this, members]() -> ConversationId {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (!db.transaction())
            return 0;

        ConversationId id = 0;
        {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "INSERT INTO conversations (kind, direct_key, created_ms) VALUES (?, NULL, ?)"));
            q.addBindValue(int(ConversationRecord::Kind::Meeting));
            q.addBindValue(QDateTime::currentMSecsSinceEpoch());
            if (!q.exec()) {
                db.rollback();
                qCWarning(lcStorage) << "createMeetingConversation failed:" << q.lastError().text();
                return 0;
            }
            id = q.lastInsertId().toULongLong();
        }

        for (UserId member : members) {
            if (member == 0)
                continue;
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO conversation_members (conversation_id, user_id) VALUES (?, ?)"));
            q.addBindValue(qint64(id));
            q.addBindValue(qint64(member));
            if (!q.exec()) {
                db.rollback();
                return 0;
            }
        }

        if (!db.commit())
            return 0;
        return id;
    });
}

QFuture<bool> StorageManager::addMember(ConversationId conversation, UserId user)
{
    return post<bool>([this, conversation, user] {
        if (conversation == 0 || user == 0)
            return false;
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO conversation_members (conversation_id, user_id) VALUES (?, ?)"));
        q.addBindValue(qint64(conversation));
        q.addBindValue(qint64(user));
        if (!q.exec()) {
            qCWarning(lcStorage) << "addMember failed:" << q.lastError().text();
            return false;
        }
        return true;
    });
}

QFuture<bool> StorageManager::isMember(ConversationId conversation, UserId user)
{
    return post<bool>([this, conversation, user] {
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral(
            "SELECT 1 FROM conversation_members WHERE conversation_id = ? AND user_id = ?"));
        q.addBindValue(qint64(conversation));
        q.addBindValue(qint64(user));
        if (!q.exec()) {
            qCWarning(lcStorage) << "isMember failed:" << q.lastError().text();
            return false;
        }
        return q.next();
    });
}

QFuture<QList<UserId>> StorageManager::members(ConversationId conversation)
{
    return post<QList<UserId>>([this, conversation] {
        QList<UserId> out;
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral(
            "SELECT user_id FROM conversation_members WHERE conversation_id = ?"));
        q.addBindValue(qint64(conversation));
        if (!q.exec()) {
            qCWarning(lcStorage) << "members failed:" << q.lastError().text();
            return out;
        }
        while (q.next())
            out.append(q.value(0).toULongLong());
        return out;
    });
}

QFuture<MessageRecord> StorageManager::saveMessage(const MessageRecord &message)
{
    return post<MessageRecord>([this, message]() -> MessageRecord {
        MessageRecord saved = message;
        if (saved.timestampMs == 0)
            saved.timestampMs = QDateTime::currentMSecsSinceEpoch();

        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);

        // Idempotent send: same client_msg_id from the same user in the same conversation.
        if (!saved.clientMsgId.isEmpty()) {
            QSqlQuery find(db);
            find.prepare(QStringLiteral(
                "SELECT id, conversation_id, from_user, client_msg_id, text, file_id, timestamp_ms "
                "FROM messages WHERE conversation_id = ? AND from_user = ? AND client_msg_id = ?"));
            find.addBindValue(qint64(saved.conversationId));
            find.addBindValue(qint64(saved.fromUser));
            find.addBindValue(saved.clientMsgId);
            if (find.exec() && find.next()) {
                saved.id = find.value(0).toULongLong();
                saved.conversationId = find.value(1).toULongLong();
                saved.fromUser = find.value(2).toULongLong();
                saved.clientMsgId = find.value(3).toString();
                saved.text = find.value(4).toString();
                saved.fileId = find.value(5).toString();
                saved.timestampMs = find.value(6).toLongLong();
                return saved;
            }
        }

        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT INTO messages (conversation_id, from_user, client_msg_id, text, file_id, timestamp_ms) "
            "VALUES (?, ?, ?, ?, ?, ?)"));
        q.addBindValue(qint64(saved.conversationId));
        q.addBindValue(qint64(saved.fromUser));
        q.addBindValue(saved.clientMsgId.isEmpty() ? QVariant() : saved.clientMsgId);
        q.addBindValue(saved.text);
        q.addBindValue(saved.fileId.isEmpty() ? QVariant() : saved.fileId);
        q.addBindValue(saved.timestampMs);
        if (!q.exec()) {
            qCWarning(lcStorage) << "saveMessage failed:" << q.lastError().text();
            return MessageRecord{};
        }
        saved.id = q.lastInsertId().toULongLong();
        return saved;
    });
}

QFuture<QList<MessageRecord>> StorageManager::history(ConversationId conversation, MessageId before, int limit)
{
    return post<QList<MessageRecord>>([this, conversation, before, limit] {
        QList<MessageRecord> newestFirst;
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        if (before == 0) {
            q.prepare(QStringLiteral(
                "SELECT id, conversation_id, from_user, client_msg_id, text, file_id, timestamp_ms "
                "FROM messages WHERE conversation_id = ? ORDER BY id DESC LIMIT ?"));
            q.addBindValue(qint64(conversation));
            q.addBindValue(limit);
        } else {
            q.prepare(QStringLiteral(
                "SELECT id, conversation_id, from_user, client_msg_id, text, file_id, timestamp_ms "
                "FROM messages WHERE conversation_id = ? AND id < ? ORDER BY id DESC LIMIT ?"));
            q.addBindValue(qint64(conversation));
            q.addBindValue(qint64(before));
            q.addBindValue(limit);
        }
        if (!q.exec()) {
            qCWarning(lcStorage) << "history failed:" << q.lastError().text();
            return QList<MessageRecord>{};
        }
        while (q.next()) {
            MessageRecord m;
            m.id = q.value(0).toULongLong();
            m.conversationId = q.value(1).toULongLong();
            m.fromUser = q.value(2).toULongLong();
            m.clientMsgId = q.value(3).toString();
            m.text = q.value(4).toString();
            m.fileId = q.value(5).toString();
            m.timestampMs = q.value(6).toLongLong();
            newestFirst.append(m);
        }

        QList<MessageRecord> oldestFirst;
        oldestFirst.reserve(newestFirst.size());
        for (auto it = newestFirst.crbegin(); it != newestFirst.crend(); ++it)
            oldestFirst.append(*it);
        return oldestFirst;
    });
}

namespace {

FileRecord readFile(const QSqlQuery &q)
{
    FileRecord file;
    file.fileId = q.value(0).toString();
    file.name = q.value(1).toString();
    file.mime = q.value(2).toString();
    file.sha256 = q.value(3).toString();
    file.blobPath = q.value(4).toString();
    file.size = q.value(5).toULongLong();
    file.received = q.value(6).toULongLong();
    file.owner = q.value(7).toULongLong();
    file.complete = q.value(8).toBool();
    file.createdMs = q.value(9).toLongLong();
    return file;
}

constexpr auto kFileColumns =
    "file_id, name, mime, sha256, blob_path, size, received, owner, complete, created_ms";

} // namespace

QFuture<bool> StorageManager::saveFile(const FileRecord &file)
{
    return post<bool>([this, file] {
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral(
            "INSERT INTO files (file_id, name, mime, sha256, blob_path, size, received, owner, complete, created_ms) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(file_id) DO UPDATE SET "
            "name=excluded.name, mime=excluded.mime, sha256=excluded.sha256, "
            "blob_path=excluded.blob_path, size=excluded.size, received=excluded.received, "
            "owner=excluded.owner, complete=excluded.complete"));
        q.addBindValue(file.fileId);
        q.addBindValue(file.name);
        q.addBindValue(file.mime);
        q.addBindValue(file.sha256);
        q.addBindValue(file.blobPath);
        q.addBindValue(qint64(file.size));
        q.addBindValue(qint64(file.received));
        q.addBindValue(qint64(file.owner));
        q.addBindValue(file.complete ? 1 : 0);
        q.addBindValue(file.createdMs == 0 ? QDateTime::currentMSecsSinceEpoch() : file.createdMs);
        if (!q.exec()) {
            qCWarning(lcStorage) << "saveFile failed:" << q.lastError().text();
            return false;
        }
        return true;
    });
}

QFuture<std::optional<FileRecord>> StorageManager::fileById(const QString &fileId)
{
    return post<std::optional<FileRecord>>([this, fileId]() -> std::optional<FileRecord> {
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral("SELECT %1 FROM files WHERE file_id = ?").arg(kFileColumns));
        q.addBindValue(fileId);
        if (!q.exec()) {
            qCWarning(lcStorage) << "fileById failed:" << q.lastError().text();
            return std::nullopt;
        }
        if (!q.next())
            return std::nullopt;
        return readFile(q);
    });
}

QFuture<std::optional<FileRecord>> StorageManager::fileBySha(const QString &sha256)
{
    return post<std::optional<FileRecord>>([this, sha256]() -> std::optional<FileRecord> {
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        // Prefer a complete blob for dedup.
        q.prepare(QStringLiteral(
            "SELECT %1 FROM files WHERE sha256 = ? ORDER BY complete DESC, created_ms ASC LIMIT 1")
                      .arg(kFileColumns));
        q.addBindValue(sha256);
        if (!q.exec()) {
            qCWarning(lcStorage) << "fileBySha failed:" << q.lastError().text();
            return std::nullopt;
        }
        if (!q.next())
            return std::nullopt;
        return readFile(q);
    });
}

QFuture<bool> StorageManager::updateFileProgress(const QString &fileId, quint64 received, bool complete)
{
    return post<bool>([this, fileId, received, complete] {
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral(
            "UPDATE files SET received = ?, complete = ? WHERE file_id = ?"));
        q.addBindValue(qint64(received));
        q.addBindValue(complete ? 1 : 0);
        q.addBindValue(fileId);
        if (!q.exec()) {
            qCWarning(lcStorage) << "updateFileProgress failed:" << q.lastError().text();
            return false;
        }
        return q.numRowsAffected() > 0;
    });
}

QFuture<bool> StorageManager::saveMeeting(const MeetingRecord &meeting)
{
    return post<bool>([this, meeting] {
        if (meeting.meetingId.isEmpty() || meeting.code.isEmpty())
            return false;
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral(
            R"(INSERT INTO meetings (meeting_id, code, title, host, conversation_id, active, created_ms)
               VALUES (?, ?, ?, ?, ?, ?, ?)
               ON CONFLICT(meeting_id) DO UPDATE SET
                 code = excluded.code,
                 title = excluded.title,
                 host = excluded.host,
                 conversation_id = excluded.conversation_id,
                 active = excluded.active)"));
        q.addBindValue(meeting.meetingId);
        q.addBindValue(meeting.code);
        q.addBindValue(meeting.title);
        q.addBindValue(qint64(meeting.host));
        q.addBindValue(meeting.conversationId ? qint64(meeting.conversationId)
                                              : QVariant());
        q.addBindValue(meeting.active ? 1 : 0);
        q.addBindValue(meeting.createdMs > 0 ? meeting.createdMs
                                             : QDateTime::currentMSecsSinceEpoch());
        if (!q.exec()) {
            qCWarning(lcStorage) << "saveMeeting failed:" << q.lastError().text();
            return false;
        }
        return true;
    });
}

QFuture<std::optional<MeetingRecord>> StorageManager::meetingByCode(const QString &code)
{
    return post<std::optional<MeetingRecord>>([this, code]() -> std::optional<MeetingRecord> {
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral(
            R"(SELECT meeting_id, code, title, host, conversation_id, active, created_ms
               FROM meetings WHERE code = ? COLLATE NOCASE AND active = 1)"));
        q.addBindValue(code.trimmed());
        if (!q.exec() || !q.next())
            return std::nullopt;
        MeetingRecord m;
        m.meetingId = q.value(0).toString();
        m.code = q.value(1).toString();
        m.title = q.value(2).toString();
        m.host = q.value(3).toULongLong();
        m.conversationId = q.value(4).toULongLong();
        m.active = q.value(5).toInt() != 0;
        m.createdMs = q.value(6).toLongLong();
        return m;
    });
}

QFuture<std::optional<MeetingRecord>> StorageManager::meetingById(const QString &meetingId)
{
    return post<std::optional<MeetingRecord>>([this, meetingId]() -> std::optional<MeetingRecord> {
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral(
            R"(SELECT meeting_id, code, title, host, conversation_id, active, created_ms
               FROM meetings WHERE meeting_id = ?)"));
        q.addBindValue(meetingId);
        if (!q.exec() || !q.next())
            return std::nullopt;
        MeetingRecord m;
        m.meetingId = q.value(0).toString();
        m.code = q.value(1).toString();
        m.title = q.value(2).toString();
        m.host = q.value(3).toULongLong();
        m.conversationId = q.value(4).toULongLong();
        m.active = q.value(5).toInt() != 0;
        m.createdMs = q.value(6).toLongLong();
        return m;
    });
}

QFuture<bool> StorageManager::closeMeeting(const QString &meetingId)
{
    return post<bool>([this, meetingId] {
        if (meetingId.isEmpty())
            return false;
        QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
        q.prepare(QStringLiteral("UPDATE meetings SET active = 0 WHERE meeting_id = ?"));
        q.addBindValue(meetingId);
        if (!q.exec()) {
            qCWarning(lcStorage) << "closeMeeting failed:" << q.lastError().text();
            return false;
        }
        return q.numRowsAffected() > 0;
    });
}

bool StorageManager::openDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_databasePath);
    if (!db.open()) {
        qCCritical(lcStorage) << "Cannot open database" << m_databasePath << db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    const char *pragmas[] = {
        "PRAGMA journal_mode=WAL",
        "PRAGMA synchronous=NORMAL",
        "PRAGMA foreign_keys=ON",
        "PRAGMA busy_timeout=5000",
    };
    for (const char *pragma : pragmas) {
        if (!q.exec(QString::fromLatin1(pragma))) {
            qCCritical(lcStorage) << pragma << "failed:" << q.lastError().text();
            return false;
        }
    }
    qCInfo(lcStorage) << "Opened database" << m_databasePath;
    return true;
}

bool StorageManager::migrate()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery q(db);

    if (!q.exec(QStringLiteral("PRAGMA user_version")) || !q.next()) {
        qCCritical(lcStorage) << "Cannot read schema version:" << q.lastError().text();
        return false;
    }
    const int version = q.value(0).toInt();
    if (version >= kSchemaVersion)
        return true;

    const QStringList v1 = {
        QStringLiteral(R"(CREATE TABLE users (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            username      TEXT    NOT NULL UNIQUE COLLATE NOCASE,
            display_name  TEXT    NOT NULL,
            password_hash BLOB    NOT NULL,
            salt          BLOB    NOT NULL,
            created_ms    INTEGER NOT NULL))"),
        QStringLiteral("CREATE INDEX idx_users_display_name ON users(display_name COLLATE NOCASE)"),

        QStringLiteral(R"(CREATE TABLE conversations (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            kind       INTEGER NOT NULL,
            direct_key TEXT    UNIQUE,
            created_ms INTEGER NOT NULL))"),

        QStringLiteral(R"(CREATE TABLE conversation_members (
            conversation_id INTEGER NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
            user_id         INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            PRIMARY KEY (conversation_id, user_id)))"),
        QStringLiteral("CREATE INDEX idx_members_user ON conversation_members(user_id)"),

        QStringLiteral(R"(CREATE TABLE messages (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            conversation_id INTEGER NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
            from_user       INTEGER NOT NULL REFERENCES users(id),
            client_msg_id   TEXT,
            text            TEXT,
            file_id         TEXT,
            timestamp_ms    INTEGER NOT NULL))"),
        QStringLiteral("CREATE INDEX idx_messages_conversation ON messages(conversation_id, id)"),

        QStringLiteral(R"(CREATE TABLE files (
            file_id    TEXT    PRIMARY KEY,
            name       TEXT    NOT NULL,
            mime       TEXT,
            sha256     TEXT,
            blob_path  TEXT,
            size       INTEGER NOT NULL,
            received   INTEGER NOT NULL DEFAULT 0,
            owner      INTEGER REFERENCES users(id),
            complete   INTEGER NOT NULL DEFAULT 0,
            created_ms INTEGER NOT NULL))"),
        QStringLiteral("CREATE INDEX idx_files_sha ON files(sha256)"),

        QStringLiteral(R"(CREATE TABLE meetings (
            meeting_id      TEXT    PRIMARY KEY,
            code            TEXT    NOT NULL UNIQUE,
            title           TEXT,
            host            INTEGER REFERENCES users(id),
            conversation_id INTEGER REFERENCES conversations(id),
            active          INTEGER NOT NULL DEFAULT 1,
            created_ms      INTEGER NOT NULL))"),

        QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion),
    };

    if (!db.transaction()) {
        qCCritical(lcStorage) << "Cannot begin migration:" << db.lastError().text();
        return false;
    }
    for (const QString &statement : v1) {
        if (!q.exec(statement)) {
            qCCritical(lcStorage) << "Migration failed:" << q.lastError().text() << statement;
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        qCCritical(lcStorage) << "Cannot commit migration:" << db.lastError().text();
        return false;
    }
    qCInfo(lcStorage) << "Database schema migrated to version" << kSchemaVersion;
    return true;
}

void StorageManager::closeDatabase()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

} // namespace qm
