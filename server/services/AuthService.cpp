#include "AuthService.h"

namespace qm {

AuthService::AuthService(StorageManager *storage, SessionRegistry *sessions,
                         HttpMediaServer *http, const ServerSettings &settings)
    : m_storage(storage)
    , m_sessions(sessions)
    , m_http(http)
    , m_settings(settings)
{
}

void AuthService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

QByteArray AuthService::hashPassword(const QString &password, const QByteArray &salt)
{
    Q_UNUSED(password)
    Q_UNUSED(salt)
    return {};
}

QByteArray AuthService::newSalt()
{
    return {};
}

void AuthService::onLogin(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void AuthService::onLogout(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

} // namespace qm
