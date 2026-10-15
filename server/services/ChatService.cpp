#include "ChatService.h"

#include "common/EnvelopeFactory.h"
#include "common/Logger.h"
#include "net/SessionRegistry.h"
#include "storage/StorageManager.h"

namespace qm {

namespace {

proto::ChatEvt toEvt(const MessageRecord &message, const std::optional<FileRecord> &file)
{
    proto::ChatEvt evt;
    evt.set_message_id(message.id);
    evt.set_conversation_id(message.conversationId);
    evt.set_from_user_id(message.fromUser);
    evt.set_client_msg_id(message.clientMsgId.toStdString());
    evt.set_text(message.text.toStdString());
    evt.set_timestamp_ms(message.timestampMs);
    if (file && !file->fileId.isEmpty()) {
        auto *att = evt.mutable_attachment();
        att->set_file_id(file->fileId.toStdString());
        att->set_name(file->name.toStdString());
        att->set_size(file->size);
        att->set_mime(file->mime.toStdString());
    }
    return evt;
}

} // namespace

ChatService::ChatService(StorageManager *storage, SessionRegistry *sessions)
    : m_storage(storage)
    , m_sessions(sessions)
{
}

void ChatService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    switch (envelope.body_case()) {
    case proto::Envelope::kChatSend:
        onSend(context, envelope);
        break;
    case proto::Envelope::kChatHistoryReq:
        onHistory(context, envelope);
        break;
    default:
        break;
    }
}

void ChatService::onSend(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &req = envelope.chat_send();
    const QString text = QString::fromStdString(req.text()).trimmed();
    const QString clientMsgId = QString::fromStdString(req.client_msg_id());
    const QString fileId = req.has_attachment()
                               ? QString::fromStdString(req.attachment().file_id())
                               : QString();

    if (text.isEmpty() && fileId.isEmpty()) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INVALID,
                                                      QStringLiteral("Empty message")));
        return;
    }

    ConversationId conversation = req.conversation_id();
    if (conversation == 0) {
        const UserId peer = req.to_user_id();
        if (peer == 0 || peer == context.user) {
            m_sessions->sendToConn(context.conn,
                                   EnvelopeFactory::error(envelope, proto::ERR_INVALID,
                                                          QStringLiteral("Missing peer")));
            return;
        }
        conversation = m_storage->directConversation(context.user, peer).result();
        if (conversation == 0) {
            m_sessions->sendToConn(context.conn,
                                   EnvelopeFactory::error(envelope, proto::ERR_INTERNAL,
                                                          QStringLiteral("Cannot open conversation")));
            return;
        }
    } else if (!m_storage->isMember(conversation, context.user).result()) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_FORBIDDEN,
                                                      QStringLiteral("Not a member")));
        return;
    }

    std::optional<FileRecord> file;
    if (!fileId.isEmpty()) {
        file = m_storage->fileById(fileId).result();
        if (!file || !file->complete) {
            m_sessions->sendToConn(context.conn,
                                   EnvelopeFactory::error(envelope, proto::ERR_NOT_FOUND,
                                                          QStringLiteral("Attachment missing")));
            return;
        }
    }

    MessageRecord record;
    record.conversationId = conversation;
    record.fromUser = context.user;
    record.clientMsgId = clientMsgId;
    record.text = text;
    record.fileId = fileId;
    record = m_storage->saveMessage(record).result();
    if (record.id == 0) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INTERNAL,
                                                      QStringLiteral("Cannot save message")));
        return;
    }

    proto::Envelope evt = EnvelopeFactory::event();
    *evt.mutable_chat_evt() = toEvt(record, file);

    const QList<UserId> memberIds = m_storage->members(conversation).result();
    m_sessions->broadcast(memberIds, evt);
}

void ChatService::onHistory(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &req = envelope.chat_history_req();
    ConversationId conversation = req.conversation_id();
    if (conversation == 0) {
        const UserId peer = req.peer_user_id();
        if (peer == 0 || peer == context.user) {
            m_sessions->sendToConn(context.conn,
                                   EnvelopeFactory::error(envelope, proto::ERR_INVALID,
                                                          QStringLiteral("Missing peer")));
            return;
        }
        conversation = m_storage->directConversation(context.user, peer).result();
        if (conversation == 0) {
            m_sessions->sendToConn(context.conn,
                                   EnvelopeFactory::error(envelope, proto::ERR_INTERNAL,
                                                          QStringLiteral("Cannot open conversation")));
            return;
        }
    } else if (!m_storage->isMember(conversation, context.user).result()) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_FORBIDDEN,
                                                      QStringLiteral("Not a member")));
        return;
    }

    const int limit = req.limit() == 0 ? 50 : int(qBound(1u, req.limit(), 100u));
    const QList<MessageRecord> rows =
        m_storage->history(conversation, req.before_message_id(), limit + 1).result();

    // history() returns oldest-first among the newest (limit+1) rows; drop the extra oldest.
    const bool hasMore = rows.size() > limit;
    const QList<MessageRecord> page = hasMore ? rows.mid(1) : rows;

    proto::Envelope reply = EnvelopeFactory::reply(envelope);
    auto *res = reply.mutable_chat_history_res();
    res->set_conversation_id(conversation);
    res->set_has_more(hasMore);

    for (const MessageRecord &message : page) {
        std::optional<FileRecord> file;
        if (!message.fileId.isEmpty())
            file = m_storage->fileById(message.fileId).result();
        *res->add_messages() = toEvt(message, file);
    }

    m_sessions->sendToConn(context.conn, reply);
}

} // namespace qm
