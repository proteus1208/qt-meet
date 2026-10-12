#include "StorageManager.h"

namespace qm {

StorageManager::StorageManager(const QString &databasePath, QObject *parent)
    : QObject(parent)
    , m_databasePath(databasePath)
{
}

StorageManager::~StorageManager()
{
    stop();
}

QFuture<bool> StorageManager::start()
{
    return {};
}

void StorageManager::stop()
{
}

QFuture<std::optional<UserRecord>> StorageManager::userByName(const QString &username)
{
    Q_UNUSED(username)
    return {};
}

QFuture<std::optional<UserRecord>> StorageManager::userById(UserId id)
{
    Q_UNUSED(id)
    return {};
}

QFuture<std::optional<UserRecord>> StorageManager::createUser(const UserRecord &user)
{
    Q_UNUSED(user)
    return {};
}

QFuture<QList<UserRecord>> StorageManager::searchUsers(const QString &query, int limit)
{
    Q_UNUSED(query)
    Q_UNUSED(limit)
    return {};
}

QFuture<ConversationId> StorageManager::directConversation(UserId a, UserId b)
{
    Q_UNUSED(a)
    Q_UNUSED(b)
    return {};
}

QFuture<ConversationId> StorageManager::createMeetingConversation(const QList<UserId> &members)
{
    Q_UNUSED(members)
    return {};
}

QFuture<bool> StorageManager::addMember(ConversationId conversation, UserId user)
{
    Q_UNUSED(conversation)
    Q_UNUSED(user)
    return {};
}

QFuture<bool> StorageManager::isMember(ConversationId conversation, UserId user)
{
    Q_UNUSED(conversation)
    Q_UNUSED(user)
    return {};
}

QFuture<QList<UserId>> StorageManager::members(ConversationId conversation)
{
    Q_UNUSED(conversation)
    return {};
}

QFuture<MessageRecord> StorageManager::saveMessage(const MessageRecord &message)
{
    Q_UNUSED(message)
    return {};
}

QFuture<QList<MessageRecord>> StorageManager::history(ConversationId conversation, MessageId before, int limit)
{
    Q_UNUSED(conversation)
    Q_UNUSED(before)
    Q_UNUSED(limit)
    return {};
}

QFuture<bool> StorageManager::saveFile(const FileRecord &file)
{
    Q_UNUSED(file)
    return {};
}

QFuture<std::optional<FileRecord>> StorageManager::fileById(const QString &fileId)
{
    Q_UNUSED(fileId)
    return {};
}

QFuture<std::optional<FileRecord>> StorageManager::fileBySha(const QString &sha256)
{
    Q_UNUSED(sha256)
    return {};
}

QFuture<bool> StorageManager::updateFileProgress(const QString &fileId, quint64 received, bool complete)
{
    Q_UNUSED(fileId)
    Q_UNUSED(received)
    Q_UNUSED(complete)
    return {};
}

QFuture<bool> StorageManager::saveMeeting(const MeetingRecord &meeting)
{
    Q_UNUSED(meeting)
    return {};
}

QFuture<std::optional<MeetingRecord>> StorageManager::meetingByCode(const QString &code)
{
    Q_UNUSED(code)
    return {};
}

QFuture<std::optional<MeetingRecord>> StorageManager::meetingById(const QString &meetingId)
{
    Q_UNUSED(meetingId)
    return {};
}

QFuture<bool> StorageManager::closeMeeting(const QString &meetingId)
{
    Q_UNUSED(meetingId)
    return {};
}

bool StorageManager::openDatabase()
{
    return false;
}

bool StorageManager::migrate()
{
    return false;
}

} // namespace qm
