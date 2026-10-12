#include "UserService.h"

#include "common/EnvelopeFactory.h"
#include "net/SessionRegistry.h"
#include "storage/InMemoryUsers.h"
#include "storage/StorageManager.h"

namespace qm {

UserService::UserService(StorageManager *storage, SessionRegistry *sessions, QObject *parent)
    : QObject(parent)
    , m_storage(storage)
    , m_sessions(sessions)
{
    Q_UNUSED(m_storage)
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
    const QString query = QString::fromStdString(envelope.user_search_req().query());
    const int limit = int(envelope.user_search_req().limit() == 0
                              ? 50
                              : envelope.user_search_req().limit());

    proto::Envelope reply = EnvelopeFactory::reply(envelope);
    auto *res = reply.mutable_user_search_res();

    const QList<UserRecord> found =
        InMemoryUsers::instance().search(query, limit, context.user);

    for (const UserRecord &user : found) {
        if (!m_sessions->isOnline(user.id))
            continue;
        auto *info = res->add_users();
        info->set_id(user.id);
        info->set_username(user.username.toStdString());
        info->set_display_name(user.displayName.toStdString());
        info->set_presence(proto::PRESENCE_ONLINE);
    }

    m_sessions->sendToConn(context.conn, reply);
}

} // namespace qm
