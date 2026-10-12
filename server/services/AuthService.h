#pragma once

#include <QByteArray>
#include <QString>

#include "common/AppConfig.h"
#include "dispatch/IHandler.h"

namespace qm {

class HttpMediaServer;
class SessionRegistry;
class StorageManager;

/// Aim: login / registration / logout; binds a connection to a user in SessionRegistry and
/// issues the session token also used for HTTP media access.
/// Handles: LoginReq, Logout.
class AuthService : public IHandler
{
public:
    AuthService(StorageManager *storage, SessionRegistry *sessions,
                HttpMediaServer *http, const ServerSettings &settings);

    bool requiresAuth() const override { return false; }
    void handle(const RequestContext &context, const proto::Envelope &envelope) override;

    static QByteArray hashPassword(const QString &password, const QByteArray &salt);
    static QByteArray newSalt();

private:
    void onLogin(const RequestContext &context, const proto::Envelope &envelope);
    void onLogout(const RequestContext &context, const proto::Envelope &envelope);

    StorageManager  *m_storage = nullptr;
    SessionRegistry *m_sessions = nullptr;
    HttpMediaServer *m_http = nullptr;
    ServerSettings   m_settings;
};

} // namespace qm
