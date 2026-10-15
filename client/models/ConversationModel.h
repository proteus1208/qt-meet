#pragma once

#include <QAbstractListModel>
#include <QList>

#include "common/Ids.h"
#include "messages.pb.h"

namespace qm {

/// Aim: recent conversations list (direct chats and meeting chats), newest activity first,
/// with unread counters.
class ConversationModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        ConversationIdRole = Qt::UserRole + 1,
        PeerUserIdRole,
        TitleRole,
        LastTextRole,
        LastTimestampRole,
        UnreadRole,
    };

    explicit ConversationModel(QObject *parent = nullptr);

    int                    rowCount(const QModelIndex &parent = {}) const override;
    QVariant               data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void upsertFromMessage(const proto::ChatEvt &message, const QString &title, UserId peer, bool unread);
    void markRead(ConversationId conversation);
    void clear();

private:
    struct Item
    {
        ConversationId id = 0;
        UserId         peer = 0;
        QString        title;
        QString        lastText;
        qint64         lastTimestamp = 0;
        int            unread = 0;
    };

    QList<Item> m_items;
};

} // namespace qm
