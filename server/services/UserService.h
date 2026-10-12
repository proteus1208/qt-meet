#pragma once

#include <QObject>

#include "dispatch/IHandler.h"

namespace qm {

class SessionRegistry;
class StorageManager;

/// Aim: user search and presence fan-out (online / offline / in-call).
/// Handles: UserSearchReq. Emits: PresenceEvt.
class UserService : public QObject, public IHandler
{
    Q_OBJECT

public:
    UserService(StorageManager *storage, SessionRegistry *sessions, QObject *parent = nullptr);

    void handle(const RequestContext &context, const proto::Envelope &envelope) override;

    void publishPresence(qm::UserId user, proto::Presence presence);

public Q_SLOTS:
    void onUserOnline(qm::UserId user);
    void onUserOffline(qm::UserId user);

private:
    void onSearch(const RequestContext &context, const proto::Envelope &envelope);

    StorageManager  *m_storage = nullptr;
    SessionRegistry *m_sessions = nullptr;
};

} // namespace qm
