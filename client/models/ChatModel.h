#pragma once

#include <QAbstractListModel>
#include <QList>

#include "common/Ids.h"
#include "messages.pb.h"

namespace qm {

/// Aim: messages of the open conversation, oldest first; supports optimistic local echo
/// (pending until the server's ChatEvt with the same client_msg_id arrives).
class ChatModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)

public:
    enum Role {
        MessageIdRole = Qt::UserRole + 1,
        FromUserIdRole,
        FromMeRole,
        TextRole,
        TimestampRole,
        PendingRole,
        FileIdRole,
        FileNameRole,
        FileSizeRole,
    };

    explicit ChatModel(QObject *parent = nullptr);

    int                    rowCount(const QModelIndex &parent = {}) const override;
    QVariant               data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setContext(ConversationId conversation, UserId me);
    void appendLocal(const QString &clientMsgId, const QString &text, const proto::ChatAttachment &attachment);
    void appendOrConfirm(const proto::ChatEvt &message);
    void prependHistory(const QList<proto::ChatEvt> &messages, bool hasMore);
    void clear();

    ConversationId conversation() const;
    MessageId      oldestMessageId() const;
    bool           hasMore() const;

Q_SIGNALS:
    void hasMoreChanged();

private:
    struct Item
    {
        proto::ChatEvt message;
        bool           pending = false;
    };

    QList<Item>    m_items;
    ConversationId m_conversation = 0;
    UserId         m_me = 0;
    bool           m_hasMore = false;
};

} // namespace qm
