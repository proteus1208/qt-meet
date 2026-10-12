#include "ChatController.h"

#include <QFutureWatcher>
#include <QTimer>

#include "common/EnvelopeFactory.h"
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
    connect(m_session, &SessionController::sessionChanged, this, [this] {
        if (m_session->loggedIn())
            return;
        m_searchDebounce->stop();
        m_latestSearchSeq = 0;
        m_searchResults->clear();
        openConversationWith(0, {});
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
    Q_EMIT activeConversationChanged();
    Q_EMIT activePeerPresenceChanged();
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
    Q_UNUSED(message)
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

    // A user we have never listed (e.g. just registered) came online: refresh the results.
    if (presence.presence() != proto::PRESENCE_OFFLINE && user != m_session->userId()
        && m_session->loggedIn()) {
        m_searchDebounce->start();
    }
}

} // namespace qm
