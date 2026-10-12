#pragma once

#include <QFuture>
#include <QObject>
#include <QThread>

#include <functional>
#include <optional>

#include "StorageTypes.h"

namespace qm {

/// Aim: the single owner of the SQLite database. All queries run on its private thread in
/// submission order; callers on any thread get a QFuture. No other class touches QSqlDatabase.
class StorageManager : public QObject
{
    Q_OBJECT

public:
    explicit StorageManager(const QString &databasePath, QObject *parent = nullptr);
    ~StorageManager() override;

    QFuture<bool> start();     // spawns the thread, opens the db, applies migrations
    void          stop();

    // users
    QFuture<std::optional<UserRecord>> userByName(const QString &username);
    QFuture<std::optional<UserRecord>> userById(UserId id);
    QFuture<std::optional<UserRecord>> createUser(const UserRecord &user);
    QFuture<QList<UserRecord>>         searchUsers(const QString &query, int limit, UserId excludeId = 0);

    // conversations + messages
    QFuture<ConversationId>         directConversation(UserId a, UserId b);   // get or create
    QFuture<ConversationId>         createMeetingConversation(const QList<UserId> &members);
    QFuture<bool>                   addMember(ConversationId conversation, UserId user);
    QFuture<bool>                   isMember(ConversationId conversation, UserId user);
    QFuture<QList<UserId>>          members(ConversationId conversation);
    QFuture<MessageRecord>          saveMessage(const MessageRecord &message);
    QFuture<QList<MessageRecord>>   history(ConversationId conversation, MessageId before, int limit);

    // files
    QFuture<bool>                      saveFile(const FileRecord &file);
    QFuture<std::optional<FileRecord>> fileById(const QString &fileId);
    QFuture<std::optional<FileRecord>> fileBySha(const QString &sha256);
    QFuture<bool>                      updateFileProgress(const QString &fileId, quint64 received, bool complete);

    // meetings
    QFuture<bool>                         saveMeeting(const MeetingRecord &meeting);
    QFuture<std::optional<MeetingRecord>> meetingByCode(const QString &code);
    QFuture<std::optional<MeetingRecord>> meetingById(const QString &meetingId);
    QFuture<bool>                         closeMeeting(const QString &meetingId);

private:
    template <typename T>
    QFuture<T> post(std::function<T()> job);

    bool openDatabase();
    bool migrate();
    void closeDatabase();

    QString m_databasePath;
    QString m_connectionName;
    QThread m_thread;
    QObject *m_worker = nullptr;     // lives in m_thread; jobs are queued to it
};

} // namespace qm
