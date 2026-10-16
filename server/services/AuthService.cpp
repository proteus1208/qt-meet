#include "AuthService.h"

#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>

#include "common/EnvelopeFactory.h"
#include "common/Ids.h"
#include "common/Logger.h"
#include "net/SessionRegistry.h"
#include "storage/StorageManager.h"
#include "streaming/HttpMediaServer.h"

namespace qm {

namespace {

constexpr int kPbkdf2Iterations = 100000;
constexpr int kHashBytes = 32;
constexpr int kSaltBytes = 16;

bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    char diff = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        diff |= char(a[i] ^ b[i]);
    return diff == 0;
}

} // namespace

AuthService::AuthService(StorageManager *storage, SessionRegistry *sessions,
                         HttpMediaServer *http, const ServerSettings &settings)
    : m_storage(storage)
    , m_sessions(sessions)
    , m_http(http)
    , m_settings(settings)
{
}

void AuthService::setSettings(const ServerSettings &settings)
{
    m_settings = settings;
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
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, password.toUtf8(), salt,
                                              kPbkdf2Iterations, kHashBytes);
}

QByteArray AuthService::newSalt()
{
    QByteArray salt(kSaltBytes, Qt::Uninitialized);
    for (int i = 0; i < salt.size(); ++i)
        salt[i] = char(QRandomGenerator::system()->bounded(256));
    return salt;
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

    std::optional<UserRecord> user = m_storage->userByName(username).result();

    if (req.register_new()) {
        if (user) {
            res->set_ok(false);
            res->set_error(proto::ERR_CONFLICT);
            m_sessions->sendToConn(context.conn, reply);
            return;
        }
        UserRecord record;
        record.username = username;
        record.displayName = displayName.isEmpty() ? username : displayName;
        record.salt = newSalt();
        record.passwordHash = hashPassword(password, record.salt);
        user = m_storage->createUser(record).result();
        if (!user) {
            // A concurrent registration may have taken the name between lookup and insert.
            const bool taken = m_storage->userByName(username).result().has_value();
            res->set_ok(false);
            res->set_error(taken ? proto::ERR_CONFLICT : proto::ERR_INTERNAL);
            m_sessions->sendToConn(context.conn, reply);
            return;
        }
        qCInfo(lcApp) << "Registered user" << username << "id" << user->id;
    } else {
        if (!user || !constantTimeEquals(user->passwordHash, hashPassword(password, user->salt))) {
            res->set_ok(false);
            res->set_error(proto::ERR_UNAUTHORIZED);
            m_sessions->sendToConn(context.conn, reply);
            return;
        }
    }

    m_sessions->bind(context.conn, user->id, user->displayName);
    const QString token = m_http ? m_http->issueToken(user->id) : Ids::uuid();
    // Only advertise a real listening base URL — a fake 127.0.0.1:port makes the
    // client fail with FFmpeg "Error number -138" (connection refused) on play.
    const QString httpBase = (m_http && m_http->isListening()) ? m_http->baseUrl() : QString();

    res->set_ok(true);
    res->set_error(proto::ERR_NONE);
    res->set_session_token(token.toStdString());
    res->set_http_base_url(httpBase.toStdString());
    res->set_rtc_port_min(m_settings.rtcPortMin);
    res->set_rtc_port_max(m_settings.rtcPortMax);
    res->set_transport_mode(m_settings.transportMode == TransportMode::Multicast
                                ? proto::TRANSPORT_MULTICAST
                                : proto::TRANSPORT_DIRECT);

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

    if (m_http)
        m_http->revokeUser(context.user);
    m_sessions->unbind(context.conn);

    proto::Envelope evt = EnvelopeFactory::event();
    auto *presence = evt.mutable_presence_evt();
    presence->set_user_id(context.user);
    presence->set_presence(proto::PRESENCE_OFFLINE);
    m_sessions->broadcastAll(evt);
}

} // namespace qm
