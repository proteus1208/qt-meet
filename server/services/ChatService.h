#pragma once

#include "dispatch/IHandler.h"

namespace qm {

class SessionRegistry;
class StorageManager;

/// Aim: direct and meeting chat: persist message, then deliver ChatEvt to every member's
/// connections (sender's other devices included). Attachments reference FileService file ids.
/// Handles: ChatSend, ChatHistoryReq.
class ChatService : public IHandler
{
public:
    ChatService(StorageManager *storage, SessionRegistry *sessions);

    void handle(const RequestContext &context, const proto::Envelope &envelope) override;

private:
    void onSend(const RequestContext &context, const proto::Envelope &envelope);
    void onHistory(const RequestContext &context, const proto::Envelope &envelope);

    StorageManager  *m_storage = nullptr;
    SessionRegistry *m_sessions = nullptr;
};

} // namespace qm
