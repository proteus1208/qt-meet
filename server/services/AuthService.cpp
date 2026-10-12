#include "AuthService.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

#include "common/EnvelopeFactory.h"
#include "common/Ids.h"
#include "common/Logger.h"
#include "net/SessionRegistry.h"
#include "storage/InMemoryUsers.h"
#include "storage/StorageManager.h"
#include "streaming/HttpMediaServer.h"

namespace qm {

AuthService::AuthService(StorageManager *storage, SessionRegistry *sessions,
                         HttpMediaServer *http, const ServerSettings &settings)
    : m_storage(storage)
    , m_sessions(sessions)
    , m_http(http)
    , m_settings(settings)
{
    Q_UNUSED(m_storage)
}

void AuthService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    switch (envelope.body_case()) {
    case proto::Envelope::kLoginReq:
        onLogin(context, envelope);
        break;
    case proto::Envelope::kLogout:
        onLogout(context, envelope);
        break;
    default:
        break;
    }
}

QByteArray AuthService::hashPassword(const QString &password, const QByteArray &salt)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(salt);
    hash.addData(password.toUtf8());
    return hash.result().toHex();
}

QByteArray AuthService::newSalt()
{
    QByteArray salt(16, Qt::Uninitialized);
    for (int i = 0; i < salt.size(); ++i)
        salt[i] = char(QRandomGenerator::global()->bounded(256));
    return salt.toHex();
}

void AuthService::onLogin(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &req = envelope.login_req();
    const QString username = QString::fromStdString(req.username()).trimmed();
    const QString password = QString::fromStdString(req.password());
    const QString displayName = QString::fromStdString(req.display_name()).trimmed();

    proto::Envelope reply = EnvelopeFactory::reply(envelope);
    auto *res = reply.mutable_login_res();

    if (username.isEmpty() || password.isEmpty()) {
        res->set_ok(false);
        res->set_error(proto::ERR_INVALID);
        m_sessions->sendToConn(context.conn, reply);
        return;
    }

    auto &users = InMemoryUsers::instance();
    std::optional<UserRecord> user = users.findByName(username);

    if (req.register_new()) {
        if (user) {
            res->set_ok(false);
            res->set_error(proto::ERR_CONFLICT);
            m_sessions->sendToConn(context.conn, reply);
            return;
        }
        const QByteArray salt = newSalt();
        user = users.create(username,
                            displayName.isEmpty() ? username : displayName,
                            hashPassword(password, salt),
                            salt);
        qCInfo(lcApp) << "Registered user" << username << "id" << user->id;
    } else {
        if (!user || user->passwordHash != hashPassword(password, user->salt)) {
            res->set_ok(false);
            res->set_error(proto::ERR_UNAUTHORIZED);
            m_sessions->sendToConn(context.conn, reply);
            return;
        }
    }

    m_sessions->bind(context.conn, user->id);
    if (m_http)
        m_http->issueToken(user->id);

    res->set_ok(true);
    res->set_error(proto::ERR_NONE);
    res->set_session_token(Ids::uuid().toStdString());
    res->set_http_base_url(QStringLiteral("http://127.0.0.1:%1").arg(m_settings.httpPort).toStdString());
    res->set_rtc_port_min(m_settings.rtcPortMin);
    res->set_rtc_port_max(m_settings.rtcPortMax);

    auto *me = res->mutable_me();
    me->set_id(user->id);
    me->set_username(user->username.toStdString());
    me->set_display_name(user->displayName.toStdString());
    me->set_presence(proto::PRESENCE_ONLINE);

    m_sessions->sendToConn(context.conn, reply);

    proto::Envelope evt = EnvelopeFactory::event();
    auto *presence = evt.mutable_presence_evt();
    presence->set_user_id(user->id);
    presence->set_presence(proto::PRESENCE_ONLINE);
    m_sessions->broadcastAll(evt);
}

void AuthService::onLogout(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(envelope)
    if (context.user == 0)
        return;

    m_sessions->unbind(context.conn);

    proto::Envelope evt = EnvelopeFactory::event();
    auto *presence = evt.mutable_presence_evt();
    presence->set_user_id(context.user);
    presence->set_presence(proto::PRESENCE_OFFLINE);
    m_sessions->broadcastAll(evt);
}

} // namespace qm
