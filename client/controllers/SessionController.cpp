#include "SessionController.h"

#include "core/NetworkClient.h"

namespace qm {

SessionController::SessionController(NetworkClient *network, QObject *parent)
    : QObject(parent)
    , m_network(network)
{
}

bool SessionController::loggedIn() const
{
    return m_loggedIn;
}

bool SessionController::busy() const
{
    return m_busy;
}

UserId SessionController::userId() const
{
    return m_me.id();
}

qint64 SessionController::userIdForQml() const
{
    return qint64(m_me.id());
}

QString SessionController::displayName() const
{
    return QString::fromStdString(m_me.display_name());
}

QString SessionController::serverHost() const
{
    return m_serverHost;
}

void SessionController::setServerHost(const QString &host)
{
    if (m_serverHost == host)
        return;
    m_serverHost = host;
    Q_EMIT serverHostChanged();
}

bool SessionController::connected() const
{
    return m_network && m_network->state() == NetworkClient::State::Connected;
}

QString SessionController::errorString() const
{
    return m_error;
}

QString SessionController::sessionToken() const
{
    return m_token;
}

QString SessionController::httpBaseUrl() const
{
    return m_httpBaseUrl;
}

void SessionController::login(const QString &username, const QString &password)
{
    sendLogin(username, password, false, {});
}

void SessionController::registerUser(const QString &username, const QString &displayName, const QString &password)
{
    sendLogin(username, password, true, displayName);
}

void SessionController::logout()
{
}

void SessionController::sendLogin(const QString &username, const QString &password, bool registerNew,
                                  const QString &displayName)
{
    Q_UNUSED(username)
    Q_UNUSED(password)
    Q_UNUSED(registerNew)
    Q_UNUSED(displayName)
}

void SessionController::onLoginReply(const proto::Envelope &reply)
{
    Q_UNUSED(reply)
}

} // namespace qm
