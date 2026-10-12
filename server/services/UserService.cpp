#include "UserService.h"

namespace qm {

UserService::UserService(StorageManager *storage, SessionRegistry *sessions, QObject *parent)
    : QObject(parent)
    , m_storage(storage)
    , m_sessions(sessions)
{
}

void UserService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void UserService::publishPresence(UserId user, proto::Presence presence)
{
    Q_UNUSED(user)
    Q_UNUSED(presence)
}

void UserService::onUserOnline(UserId user)
{
    Q_UNUSED(user)
}

void UserService::onUserOffline(UserId user)
{
    Q_UNUSED(user)
}

void UserService::onSearch(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

} // namespace qm
