#include "ChatService.h"

namespace qm {

ChatService::ChatService(StorageManager *storage, SessionRegistry *sessions)
    : m_storage(storage)
    , m_sessions(sessions)
{
}

void ChatService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void ChatService::onSend(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void ChatService::onHistory(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

} // namespace qm
