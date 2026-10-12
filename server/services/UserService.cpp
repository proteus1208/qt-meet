#include "UserService.h"

#include "common/EnvelopeFactory.h"
#include "net/SessionRegistry.h"
#include "storage/StorageManager.h"

namespace qm {

UserService::UserService(StorageManager *storage, SessionRegistry *sessions, QObject *parent)
    : QObject(parent)
    , m_storage(storage)
    , m_sessions(sessions)
{
}

void UserService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    if (envelope.body_case() == proto::Envelope::kUserSearchReq)
        onSearch(context, envelope);
}

void UserService::publishPresence(UserId user, proto::Presence presence)
{
    proto::Envelope evt = EnvelopeFactory::event();
    auto *p = evt.mutable_presence_evt();
    p->set_user_id(user);
    p->set_presence(presence);
    m_sessions->broadcastAll(evt);
}

void UserService::onUserOnline(UserId user)
{
    publishPresence(user, proto::PRESENCE_ONLINE);
}

void UserService::onUserOffline(UserId user)
{
    publishPresence(user, proto::PRESENCE_OFFLINE);
}

void UserService::onSearch(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &req = envelope.user_search_req();
    const QString query = QString::fromStdString(req.query());
    const int limit = req.limit() == 0 ? 50 : int(qBound(1u, req.limit(), 100u));

    proto::Envelope reply = EnvelopeFactory::reply(envelope);
    auto *res = reply.mutable_user_search_res();

    const QList<UserRecord> found = m_storage->searchUsers(query, limit, context.user).result();

    for (const UserRecord &user : found) {
        auto *info = res->add_users();
        info->set_id(user.id);
        info->set_username(user.username.toStdString());
        info->set_display_name(user.displayName.toStdString());
        info->set_presence(m_sessions->isOnline(user.id) ? proto::PRESENCE_ONLINE
                                                         : proto::PRESENCE_OFFLINE);
    }

    m_sessions->sendToConn(context.conn, reply);
}

} // namespace qm
