#include "SessionController.h"

#include <QFutureWatcher>

#include "common/EnvelopeFactory.h"
#include "common/Logger.h"
#include "core/NetworkClient.h"

namespace qm {

SessionController::SessionController(NetworkClient *network, QObject *parent)
    : QObject(parent)
    , m_network(network)
{
    connect(m_network, &NetworkClient::stateChanged, this, [this](NetworkClient::State) {
        Q_EMIT connectedChanged();
    });
    connect(m_network, &NetworkClient::errorOccurred, this, [this](const QString &msg) {
        m_error = msg;
        Q_EMIT errorChanged();
    });
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

int SessionController::serverPort() const
{
    return int(m_serverPort);
}

void SessionController::setServerPort(int port)
{
    if (m_serverPort == quint16(port))
        return;
    m_serverPort = quint16(port);
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

void SessionController::registerUser(const QString &username, const QString &displayName,
                                     const QString &password)
{
    sendLogin(username, password, true, displayName);
}

void SessionController::logout()
{
    if (m_network && m_loggedIn) {
        proto::Envelope env = EnvelopeFactory::request();
        env.mutable_logout();
        m_network->post(env);
    }
    m_loggedIn = false;
    m_token.clear();
    m_me.Clear();
    Q_EMIT sessionChanged();
}

void SessionController::sendLogin(const QString &username, const QString &password, bool registerNew,
                                  const QString &displayName)
{
    if (m_busy)
        return;

    m_busy = true;
    m_error.clear();
    Q_EMIT busyChanged();
    Q_EMIT errorChanged();

    if (m_serverHost.trimmed().isEmpty()) {
        m_busy = false;
        m_error = QStringLiteral("Server address is empty");
        Q_EMIT busyChanged();
        Q_EMIT errorChanged();
        Q_EMIT loginFailed(m_error);
        return;
    }

    // Connect first if needed, then send login.
    auto doRequest = [this, username, password, registerNew, displayName] {
        proto::Envelope env = EnvelopeFactory::request();
        auto *req = env.mutable_login_req();
        req->set_username(username.toStdString());
        req->set_password(password.toStdString());
        req->set_register_new(registerNew);
        req->set_display_name(displayName.toStdString());
        req->set_client_version(QStringLiteral("0.1.0").toStdString());

        auto future = m_network->request(env, 10000);
        auto *watcher = new QFutureWatcher<proto::Envelope>(this);
        connect(watcher, &QFutureWatcher<proto::Envelope>::finished, this, [this, watcher] {
            watcher->deleteLater();
            m_busy = false;
            Q_EMIT busyChanged();
            try {
                onLoginReply(watcher->result());
            } catch (const std::exception &ex) {
                m_error = QString::fromUtf8(ex.what());
                Q_EMIT errorChanged();
                Q_EMIT loginFailed(m_error);
            }
        });
        watcher->setFuture(future);
    };

    if (m_network->state() == NetworkClient::State::Connected) {
        doRequest();
        return;
    }

    QObject *context = new QObject(this);
    connect(m_network, &NetworkClient::stateChanged, context,
            [this, context, doRequest](NetworkClient::State state) {
                if (state == NetworkClient::State::Connected) {
                    context->deleteLater();
                    doRequest();
                } else if (state == NetworkClient::State::Disconnected) {
                    // wait — connection attempt in progress
                }
            });
    connect(m_network, &NetworkClient::errorOccurred, context,
            [this, context](const QString &msg) {
                context->deleteLater();
                m_busy = false;
                m_error = msg;
                Q_EMIT busyChanged();
                Q_EMIT errorChanged();
                Q_EMIT loginFailed(msg);
            });

    m_network->connectToServer(m_serverHost.trimmed(), m_serverPort);
}

void SessionController::onLoginReply(const proto::Envelope &reply)
{
    if (!reply.has_login_res()) {
        m_error = QStringLiteral("Unexpected login reply");
        Q_EMIT errorChanged();
        Q_EMIT loginFailed(m_error);
        return;
    }

    const auto &res = reply.login_res();
    if (!res.ok()) {
        m_error = QStringLiteral("Login failed (code %1)").arg(int(res.error()));
        Q_EMIT errorChanged();
        Q_EMIT loginFailed(m_error);
        return;
    }

    m_me = res.me();
    m_token = QString::fromStdString(res.session_token());
    m_httpBaseUrl = QString::fromStdString(res.http_base_url());
    m_loggedIn = true;
    m_error.clear();
    Q_EMIT errorChanged();
    Q_EMIT sessionChanged();
    qCInfo(lcApp) << "Logged in as" << displayName() << "id" << userId();
}

} // namespace qm
