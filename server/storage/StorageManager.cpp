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
    Q_UNUSED(a)
    Q_UNUSED(b)
    return QtFuture::makeReadyValueFuture(ConversationId(0));
}

QFuture<ConversationId> StorageManager::createMeetingConversation(const QList<UserId> &members)
{
    Q_UNUSED(members)
    return QtFuture::makeReadyValueFuture(ConversationId(0));
}

QFuture<bool> StorageManager::addMember(ConversationId conversation, UserId user)
{
    Q_UNUSED(conversation)
    Q_UNUSED(user)
    return QtFuture::makeReadyValueFuture(false);
}

QFuture<bool> StorageManager::isMember(ConversationId conversation, UserId user)
{
    Q_UNUSED(conversation)
    Q_UNUSED(user)
    return QtFuture::makeReadyValueFuture(false);
}

QFuture<QList<UserId>> StorageManager::members(ConversationId conversation)
{
    Q_UNUSED(conversation)
    return QtFuture::makeReadyValueFuture(QList<UserId>{});
}

QFuture<MessageRecord> StorageManager::saveMessage(const MessageRecord &message)
{
    Q_UNUSED(message)
    return QtFuture::makeReadyValueFuture(MessageRecord{});
}

QFuture<QList<MessageRecord>> StorageManager::history(ConversationId conversation, MessageId before, int limit)
{
    Q_UNUSED(conversation)
    Q_UNUSED(before)
    Q_UNUSED(limit)
    return QtFuture::makeReadyValueFuture(QList<MessageRecord>{});
}

QFuture<bool> StorageManager::saveFile(const FileRecord &file)
{
    Q_UNUSED(file)
    return QtFuture::makeReadyValueFuture(false);
}

QFuture<std::optional<FileRecord>> StorageManager::fileById(const QString &fileId)
{
    Q_UNUSED(fileId)
    return QtFuture::makeReadyValueFuture(std::optional<FileRecord>{});
}

QFuture<std::optional<FileRecord>> StorageManager::fileBySha(const QString &sha256)
{
    Q_UNUSED(sha256)
    return QtFuture::makeReadyValueFuture(std::optional<FileRecord>{});
}

QFuture<bool> StorageManager::updateFileProgress(const QString &fileId, quint64 received, bool complete)
{
    Q_UNUSED(fileId)
    Q_UNUSED(received)
    Q_UNUSED(complete)
    return QtFuture::makeReadyValueFuture(false);
}

QFuture<bool> StorageManager::saveMeeting(const MeetingRecord &meeting)
{
    Q_UNUSED(meeting)
    return QtFuture::makeReadyValueFuture(false);
}

QFuture<std::optional<MeetingRecord>> StorageManager::meetingByCode(const QString &code)
{
    Q_UNUSED(code)
    return QtFuture::makeReadyValueFuture(std::optional<MeetingRecord>{});
}

QFuture<std::optional<MeetingRecord>> StorageManager::meetingById(const QString &meetingId)
{
    Q_UNUSED(meetingId)
    return QtFuture::makeReadyValueFuture(std::optional<MeetingRecord>{});
}

QFuture<bool> StorageManager::closeMeeting(const QString &meetingId)
{
    Q_UNUSED(meetingId)
    return QtFuture::makeReadyValueFuture(false);
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
