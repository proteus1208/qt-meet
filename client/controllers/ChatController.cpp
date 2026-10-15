#include "ChatController.h"

#include <QFutureWatcher>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include "common/EnvelopeFactory.h"
#include "common/Ids.h"
#include "common/Logger.h"
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
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(250);
    connect(m_searchDebounce, &QTimer::timeout, this, &ChatController::sendSearch);

    connect(m_network, &NetworkClient::event, this, &ChatController::onEvent);
    connect(m_messages, &ChatModel::hasMoreChanged, this, &ChatController::hasMoreMessagesChanged);
    connect(m_session, &SessionController::sessionChanged, this, [this] {
        if (m_session->loggedIn()) {
            m_files->setHttpAccess(m_session->httpBaseUrl(), m_session->sessionToken());
            return;
        }
        m_searchDebounce->stop();
        m_latestSearchSeq = 0;
        m_searchResults->clear();
        m_conversations->clear();
        m_messages->clear();
        m_files->setHttpAccess({}, {});
        openConversationWith(0, {});
    });

    connect(m_files, &FileTransfer::uploaded, this,
            [this](const QString &, const proto::ChatAttachment &attachment) {
                if (m_activePeer == 0)
                    return;
                const QString clientMsgId = Ids::uuid();
                m_messages->appendLocal(clientMsgId, {}, attachment);

                proto::Envelope env = EnvelopeFactory::request();
                auto *send = env.mutable_chat_send();
                send->set_conversation_id(m_activeConversation);
                send->set_to_user_id(m_activePeer);
                send->set_client_msg_id(clientMsgId.toStdString());
                *send->mutable_attachment() = attachment;
                m_network->post(env);
            });
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

int ChatController::activePeerPresence() const
{
    return int(m_activePeerPresence);
}

bool ChatController::hasMoreMessages() const
{
    return m_messages && m_messages->hasMore();
}

void ChatController::searchUsers(const QString &query)
{
    m_searchQuery = query.trimmed();
    m_searchDebounce->start();
}

void ChatController::sendSearch()
{
    if (!m_session->loggedIn())
        return;

    proto::Envelope env = EnvelopeFactory::request();
    if (env.seq() == 0)
        env.set_seq(EnvelopeFactory::nextSeq());
    auto *req = env.mutable_user_search_req();
    req->set_query(m_searchQuery.toStdString());
    req->set_limit(50);

    const quint64 seq = env.seq();
    m_latestSearchSeq = seq;

    auto *watcher = new QFutureWatcher<proto::Envelope>(this);
    connect(watcher, &QFutureWatcher<proto::Envelope>::finished, this, [this, watcher, seq] {
        watcher->deleteLater();
        if (seq != m_latestSearchSeq)
            return;

        proto::Envelope reply;
        try {
            reply = watcher->result();
        } catch (const std::exception &ex) {
            qCWarning(lcNet) << "User search failed:" << ex.what();
            return;
        }
        if (!reply.has_user_search_res())
            return;

        QList<proto::UserInfo> users;
        users.reserve(reply.user_search_res().users_size());
        for (const proto::UserInfo &user : reply.user_search_res().users())
            users.append(user);
        m_searchResults->setUsers(users);
    });
    watcher->setFuture(m_network->request(env));
}

void ChatController::openConversationWith(qint64 userId, const QString &displayName)
{
    m_activePeer = UserId(userId);
    m_activePeerName = displayName;
    m_activePeerPresence = m_searchResults->presenceOf(m_activePeer);
    m_activeConversation = 0;
    m_messages->clear();
    m_messages->setContext(0, m_session->userId());
    Q_EMIT activeConversationChanged();
    Q_EMIT activePeerPresenceChanged();

    if (m_activePeer == 0 || !m_session->loggedIn())
        return;

    proto::Envelope env = EnvelopeFactory::request();
    auto *req = env.mutable_chat_history_req();
    req->set_peer_user_id(m_activePeer);
    req->set_limit(50);

    auto *watcher = new QFutureWatcher<proto::Envelope>(this);
    connect(watcher, &QFutureWatcher<proto::Envelope>::finished, this,
            [this, watcher, peer = m_activePeer] {
                watcher->deleteLater();
                if (peer != m_activePeer)
                    return;
                proto::Envelope reply;
                try {
                    reply = watcher->result();
                } catch (const std::exception &ex) {
                    qCWarning(lcNet) << "History failed:" << ex.what();
                    return;
                }
                if (reply.has_error() || !reply.has_chat_history_res())
                    return;

                const auto &res = reply.chat_history_res();
                m_activeConversation = res.conversation_id();
                m_messages->setContext(m_activeConversation, m_session->userId());
                QList<proto::ChatEvt> messages;
                messages.reserve(res.messages_size());
                for (const proto::ChatEvt &message : res.messages())
                    messages.append(message);
                m_messages->prependHistory(messages, res.has_more());
                m_conversations->markRead(m_activeConversation);
                Q_EMIT activeConversationChanged();
            });
    watcher->setFuture(m_network->request(env));
}

void ChatController::sendText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || m_activePeer == 0 || !m_session->loggedIn())
        return;

    const QString clientMsgId = Ids::uuid();
    m_messages->appendLocal(clientMsgId, trimmed, {});

    proto::Envelope env = EnvelopeFactory::request();
    auto *send = env.mutable_chat_send();
    send->set_conversation_id(m_activeConversation);
    send->set_to_user_id(m_activePeer);
    send->set_client_msg_id(clientMsgId.toStdString());
    send->set_text(trimmed.toStdString());
    m_network->post(env);
}

void ChatController::sendFile(const QUrl &localFile)
{
    if (m_activePeer == 0 || !m_session->loggedIn())
        return;
    const QString path = localFile.isLocalFile() ? localFile.toLocalFile() : localFile.toString();
    if (path.isEmpty())
        return;
    m_files->upload(path, m_activeConversation, m_activePeer);
}

void ChatController::loadMore()
{
    if (m_activePeer == 0 || !m_session->loggedIn() || !m_messages->hasMore())
        return;

    proto::Envelope env = EnvelopeFactory::request();
    auto *req = env.mutable_chat_history_req();
    req->set_conversation_id(m_activeConversation);
    req->set_peer_user_id(m_activePeer);
    req->set_before_message_id(m_messages->oldestMessageId());
    req->set_limit(50);

    auto *watcher = new QFutureWatcher<proto::Envelope>(this);
    connect(watcher, &QFutureWatcher<proto::Envelope>::finished, this,
            [this, watcher, peer = m_activePeer] {
                watcher->deleteLater();
                if (peer != m_activePeer)
                    return;
                proto::Envelope reply;
                try {
                    reply = watcher->result();
                } catch (const std::exception &) {
                    return;
                }
                if (!reply.has_chat_history_res())
                    return;
                const auto &res = reply.chat_history_res();
                QList<proto::ChatEvt> messages;
                for (const proto::ChatEvt &message : res.messages())
                    messages.append(message);
                m_messages->prependHistory(messages, res.has_more());
            });
    watcher->setFuture(m_network->request(env));
}

void ChatController::downloadAttachment(const QString &fileId, const QString &name)
{
    if (fileId.isEmpty())
        return;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    m_files->download(fileId, name, dir);
}

void ChatController::onEvent(const proto::Envelope &envelope)
{
    switch (envelope.body_case()) {
    case proto::Envelope::kPresenceEvt:
        onPresence(envelope.presence_evt());
        break;
    case proto::Envelope::kChatEvt:
        onChatEvent(envelope.chat_evt());
        break;
    default:
        break;
    }
}

void ChatController::onChatEvent(const proto::ChatEvt &message)
{
    const UserId me = m_session->userId();
    const UserId from = message.from_user_id();
    const bool fromMe = from == me;
    const ConversationId conv = message.conversation_id();

    auto displayNameOf = [this](UserId user) -> QString {
        for (int i = 0; i < m_searchResults->rowCount(); ++i) {
            const QVariantMap row = m_searchResults->get(i);
            if (row.value(QStringLiteral("userId")).toLongLong() == qint64(user))
                return row.value(QStringLiteral("displayName")).toString();
        }
        return QStringLiteral("User %1").arg(user);
    };

    UserId peer = fromMe ? m_activePeer : from;
    QString title = fromMe ? m_activePeerName : displayNameOf(from);
    if (conv != 0 && conv == m_activeConversation) {
        peer = m_activePeer;
        title = m_activePeerName;
    }

    const bool inActive = m_activePeer != 0
                          && (conv == m_activeConversation
                              || (m_activeConversation == 0 && (from == m_activePeer || fromMe)));
    if (inActive) {
        if (m_activeConversation == 0 && conv != 0)
            m_activeConversation = conv;
        m_messages->setContext(m_activeConversation, me);
        m_messages->appendOrConfirm(message);
        if (conv != 0)
            m_conversations->markRead(conv);
    }

    if (title.isEmpty())
        title = QStringLiteral("Chat");
    const bool unread = !fromMe && !inActive;
    m_conversations->upsertFromMessage(message, title, peer, unread);

    if (!fromMe)
        Q_EMIT incomingMessage(title, QString::fromStdString(message.text()));
}

void ChatController::onPresence(const proto::PresenceEvt &presence)
{
    const UserId user = presence.user_id();
    if (user != 0 && user == m_activePeer && m_activePeerPresence != presence.presence()) {
        m_activePeerPresence = presence.presence();
        Q_EMIT activePeerPresenceChanged();
    }
    if (m_searchResults->updatePresence(user, presence.presence()))
        return;

    if (presence.presence() != proto::PRESENCE_OFFLINE && user != m_session->userId()
        && m_session->loggedIn()) {
        m_searchDebounce->start();
    }
}

} // namespace qm
