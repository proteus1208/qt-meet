#include "ChatModel.h"

#include <QDateTime>

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
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const Item &item = m_items.at(index.row());
    const proto::ChatEvt &message = item.message;
    switch (role) {
    case MessageIdRole:
        return qint64(message.message_id());
    case FromUserIdRole:
        return qint64(message.from_user_id());
    case FromMeRole:
        return message.from_user_id() == m_me;
    case TextRole:
        return QString::fromStdString(message.text());
    case TimestampRole:
        return qint64(message.timestamp_ms());
    case PendingRole:
        return item.pending;
    case FileIdRole:
        return message.has_attachment() ? QString::fromStdString(message.attachment().file_id())
                                        : QString();
    case FileNameRole:
        return message.has_attachment() ? QString::fromStdString(message.attachment().name())
                                        : QString();
    case FileSizeRole:
        return message.has_attachment() ? qint64(message.attachment().size()) : qint64(0);
    default:
        return {};
    }
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

void ChatModel::appendLocal(const QString &clientMsgId, const QString &text,
                            const proto::ChatAttachment &attachment)
{
    Item item;
    item.pending = true;
    item.message.set_client_msg_id(clientMsgId.toStdString());
    item.message.set_from_user_id(m_me);
    item.message.set_conversation_id(m_conversation);
    item.message.set_text(text.toStdString());
    item.message.set_timestamp_ms(QDateTime::currentMSecsSinceEpoch());
    if (!attachment.file_id().empty() || !attachment.name().empty())
        *item.message.mutable_attachment() = attachment;

    const int row = int(m_items.size());
    beginInsertRows({}, row, row);
    m_items.append(item);
    endInsertRows();
}

void ChatModel::appendOrConfirm(const proto::ChatEvt &message)
{
    if (m_conversation != 0 && message.conversation_id() != 0
        && message.conversation_id() != m_conversation) {
        return;
    }
    if (m_conversation == 0 && message.conversation_id() != 0)
        m_conversation = message.conversation_id();

    const QString clientId = QString::fromStdString(message.client_msg_id());
    if (!clientId.isEmpty()) {
        for (int row = 0; row < m_items.size(); ++row) {
            if (m_items[row].message.client_msg_id() != message.client_msg_id())
                continue;
            m_items[row].message = message;
            m_items[row].pending = false;
            const QModelIndex idx = index(row);
            Q_EMIT dataChanged(idx, idx);
            return;
        }
    }

    for (const Item &item : m_items) {
        if (message.message_id() != 0 && item.message.message_id() == message.message_id())
            return;
    }

    const int row = int(m_items.size());
    beginInsertRows({}, row, row);
    m_items.append({message, false});
    endInsertRows();
}

void ChatModel::prependHistory(const QList<proto::ChatEvt> &messages, bool hasMore)
{
    if (m_hasMore != hasMore) {
        m_hasMore = hasMore;
        Q_EMIT hasMoreChanged();
    }
    if (messages.isEmpty())
        return;

    QList<Item> fresh;
    fresh.reserve(messages.size());
    for (const proto::ChatEvt &message : messages) {
        bool known = false;
        for (const Item &item : m_items) {
            if ((message.message_id() != 0 && item.message.message_id() == message.message_id())
                || (!message.client_msg_id().empty()
                    && item.message.client_msg_id() == message.client_msg_id())) {
                known = true;
                break;
            }
        }
        if (!known)
            fresh.append({message, false});
    }
    if (fresh.isEmpty())
        return;

    beginInsertRows({}, 0, int(fresh.size()) - 1);
    for (int i = fresh.size() - 1; i >= 0; --i)
        m_items.prepend(fresh.at(i));
    endInsertRows();
}

void ChatModel::clear()
{
    if (m_items.isEmpty() && !m_hasMore) {
        m_conversation = 0;
        return;
    }
    beginResetModel();
    m_items.clear();
    m_conversation = 0;
    endResetModel();
    if (m_hasMore) {
        m_hasMore = false;
        Q_EMIT hasMoreChanged();
    }
}

ConversationId ChatModel::conversation() const
{
    return m_conversation;
}

MessageId ChatModel::oldestMessageId() const
{
    for (const Item &item : m_items) {
        if (item.message.message_id() != 0)
            return item.message.message_id();
    }
    return 0;
}

bool ChatModel::hasMore() const
{
    return m_hasMore;
}

} // namespace qm
