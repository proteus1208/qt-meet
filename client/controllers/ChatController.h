#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QUrl>

#include "common/Ids.h"
#include "messages.pb.h"

namespace qm {

class ChatModel;
class ConversationModel;
class FileTransfer;
class NetworkClient;
class SessionController;
class TransferModel;
class UserListModel;

/// Aim: user search, conversations and messaging for QML, including file attachments.
class ChatController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *searchResults READ searchResults CONSTANT)
    Q_PROPERTY(QAbstractItemModel *conversations READ conversations CONSTANT)
    Q_PROPERTY(QAbstractItemModel *messages READ messages CONSTANT)
    Q_PROPERTY(QAbstractItemModel *transfers READ transfers CONSTANT)
    Q_PROPERTY(qint64 activePeerId READ activePeerId NOTIFY activeConversationChanged)
    Q_PROPERTY(QString activePeerName READ activePeerName NOTIFY activeConversationChanged)

public:
    ChatController(NetworkClient *network, SessionController *session, FileTransfer *files,
                   UserListModel *searchResults, ConversationModel *conversations,
                   ChatModel *messages, TransferModel *transfers, QObject *parent = nullptr);

    QAbstractItemModel *searchResults() const;
    QAbstractItemModel *conversations() const;
    QAbstractItemModel *messages() const;
    QAbstractItemModel *transfers() const;
    qint64              activePeerId() const;
    QString             activePeerName() const;

    Q_INVOKABLE void searchUsers(const QString &query);
    Q_INVOKABLE void openConversationWith(qint64 userId, const QString &displayName);
    Q_INVOKABLE void sendText(const QString &text);
    Q_INVOKABLE void sendFile(const QUrl &localFile);
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void downloadAttachment(const QString &fileId, const QString &name);

Q_SIGNALS:
    void activeConversationChanged();
    void incomingMessage(const QString &fromName, const QString &text);

private:
    void onEvent(const proto::Envelope &envelope);
    void onChatEvent(const proto::ChatEvt &message);
    void onPresence(const proto::PresenceEvt &presence);

    NetworkClient     *m_network = nullptr;
    SessionController *m_session = nullptr;
    FileTransfer      *m_files = nullptr;
    UserListModel     *m_searchResults = nullptr;
    ConversationModel *m_conversations = nullptr;
    ChatModel         *m_messages = nullptr;
    TransferModel     *m_transfers = nullptr;
    UserId             m_activePeer = 0;
    QString            m_activePeerName;
    ConversationId     m_activeConversation = 0;
};

} // namespace qm
