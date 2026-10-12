#include "ChatController.h"

#include "controllers/SessionController.h"
#include "core/FileTransfer.h"
#include "core/NetworkClient.h"
#include "models/ChatModel.h"
#include "models/ConversationModel.h"
#include "models/TransferModel.h"
#include "models/UserListModel.h"

namespace qm {

ChatController::ChatController(NetworkClient *network, SessionController *session, FileTransfer *files,
                               UserListModel *searchResults, ConversationModel *conversations,
                               ChatModel *messages, TransferModel *transfers, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_session(session)
    , m_files(files)
    , m_searchResults(searchResults)
    , m_conversations(conversations)
    , m_messages(messages)
    , m_transfers(transfers)
{
}

QAbstractItemModel *ChatController::searchResults() const
{
    return m_searchResults;
}

QAbstractItemModel *ChatController::conversations() const
{
    return m_conversations;
}

QAbstractItemModel *ChatController::messages() const
{
    return m_messages;
}

QAbstractItemModel *ChatController::transfers() const
{
    return m_transfers;
}

qint64 ChatController::activePeerId() const
{
    return qint64(m_activePeer);
}

QString ChatController::activePeerName() const
{
    return m_activePeerName;
}

void ChatController::searchUsers(const QString &query)
{
    Q_UNUSED(query)
}

void ChatController::openConversationWith(qint64 userId, const QString &displayName)
{
    m_activePeer = UserId(userId);
    m_activePeerName = displayName;
    Q_EMIT activeConversationChanged();
}

void ChatController::sendText(const QString &text)
{
    Q_UNUSED(text)
}

void ChatController::sendFile(const QUrl &localFile)
{
    Q_UNUSED(localFile)
}

void ChatController::loadMore()
{
}

void ChatController::downloadAttachment(const QString &fileId, const QString &name)
{
    Q_UNUSED(fileId)
    Q_UNUSED(name)
}

void ChatController::onEvent(const proto::Envelope &envelope)
{
    Q_UNUSED(envelope)
}

void ChatController::onChatEvent(const proto::ChatEvt &message)
{
    Q_UNUSED(message)
}

void ChatController::onPresence(const proto::PresenceEvt &presence)
{
    Q_UNUSED(presence)
}

} // namespace qm
