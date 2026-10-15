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
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const Item &item = m_items.at(index.row());
    switch (role) {
    case ConversationIdRole:
        return qint64(item.id);
    case PeerUserIdRole:
        return qint64(item.peer);
    case Qt::DisplayRole:
    case TitleRole:
        return item.title;
    case LastTextRole:
        return item.lastText;
    case LastTimestampRole:
        return item.lastTimestamp;
    case UnreadRole:
        return item.unread;
    default:
        return {};
    }
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

void ConversationModel::upsertFromMessage(const proto::ChatEvt &message, const QString &title,
                                          UserId peer, bool unread)
{
    const ConversationId id = message.conversation_id();
    if (id == 0)
        return;

    QString preview = QString::fromStdString(message.text()).trimmed();
    if (preview.isEmpty() && message.has_attachment())
        preview = QStringLiteral("📎 %1").arg(QString::fromStdString(message.attachment().name()));

    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items[row].id != id)
            continue;

        beginRemoveRows({}, row, row);
        Item item = m_items.takeAt(row);
        endRemoveRows();

        item.title = title.isEmpty() ? item.title : title;
        if (peer != 0)
            item.peer = peer;
        item.lastText = preview;
        item.lastTimestamp = message.timestamp_ms();
        if (unread)
            item.unread += 1;

        beginInsertRows({}, 0, 0);
        m_items.prepend(item);
        endInsertRows();
        return;
    }

    Item item;
    item.id = id;
    item.peer = peer;
    item.title = title;
    item.lastText = preview;
    item.lastTimestamp = message.timestamp_ms();
    item.unread = unread ? 1 : 0;
    beginInsertRows({}, 0, 0);
    m_items.prepend(item);
    endInsertRows();
}

void ConversationModel::markRead(ConversationId conversation)
{
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items[row].id != conversation || m_items[row].unread == 0)
            continue;
        m_items[row].unread = 0;
        const QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, {UnreadRole});
        return;
    }
}

void ConversationModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    endResetModel();
}

} // namespace qm
