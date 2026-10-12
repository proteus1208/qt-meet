#include "ConversationModel.h"

namespace qm {

ConversationModel::ConversationModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ConversationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant ConversationModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return {};
}

QHash<int, QByteArray> ConversationModel::roleNames() const
{
    return {
        {ConversationIdRole, "conversationId"},
        {PeerUserIdRole, "peerUserId"},
        {TitleRole, "title"},
        {LastTextRole, "lastText"},
        {LastTimestampRole, "lastTimestamp"},
        {UnreadRole, "unread"},
    };
}

void ConversationModel::upsertFromMessage(const proto::ChatEvt &message, const QString &title, UserId peer, bool unread)
{
    Q_UNUSED(message)
    Q_UNUSED(title)
    Q_UNUSED(peer)
    Q_UNUSED(unread)
}

void ConversationModel::markRead(ConversationId conversation)
{
    Q_UNUSED(conversation)
}

void ConversationModel::clear()
{
}

} // namespace qm
