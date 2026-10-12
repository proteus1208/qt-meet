#include "ChatModel.h"

namespace qm {

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return {};
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    return {
        {MessageIdRole, "messageId"},
        {FromUserIdRole, "fromUserId"},
        {FromMeRole, "fromMe"},
        {TextRole, "text"},
        {TimestampRole, "timestamp"},
        {PendingRole, "pending"},
        {FileIdRole, "fileId"},
        {FileNameRole, "fileName"},
        {FileSizeRole, "fileSize"},
    };
}

void ChatModel::setContext(ConversationId conversation, UserId me)
{
    m_conversation = conversation;
    m_me = me;
}

void ChatModel::appendLocal(const QString &clientMsgId, const QString &text, const proto::ChatAttachment &attachment)
{
    Q_UNUSED(clientMsgId)
    Q_UNUSED(text)
    Q_UNUSED(attachment)
}

void ChatModel::appendOrConfirm(const proto::ChatEvt &message)
{
    Q_UNUSED(message)
}

void ChatModel::prependHistory(const QList<proto::ChatEvt> &messages, bool hasMore)
{
    Q_UNUSED(messages)
    Q_UNUSED(hasMore)
}

void ChatModel::clear()
{
}

ConversationId ChatModel::conversation() const
{
    return m_conversation;
}

MessageId ChatModel::oldestMessageId() const
{
    return 0;
}

bool ChatModel::hasMore() const
{
    return m_hasMore;
}

} // namespace qm
