#pragma once

#include <QObject>
#include <QString>

#include "common/Ids.h"
#include "messages.pb.h"

namespace qm {

class NetworkClient;

/// Aim: login / register / logout and "who am I" for QML; holds the session token and HTTP
/// base url other controllers need.
class SessionController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY sessionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(qint64 userId READ userIdForQml NOTIFY sessionChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY sessionChanged)
    Q_PROPERTY(QString serverHost READ serverHost WRITE setServerHost NOTIFY serverHostChanged)
    Q_PROPERTY(int serverPort READ serverPort WRITE setServerPort NOTIFY serverHostChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

public:
    explicit SessionController(NetworkClient *network, QObject *parent = nullptr);

    bool    loggedIn() const;
    bool    busy() const;
    UserId  userId() const;
    qint64  userIdForQml() const;
    QString displayName() const;
    QString serverHost() const;
    void    setServerHost(const QString &host);
    int     serverPort() const;
    void    setServerPort(int port);
    bool    connected() const;
    QString errorString() const;

    QString sessionToken() const;
    QString httpBaseUrl() const;

    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void registerUser(const QString &username, const QString &displayName, const QString &password);
    Q_INVOKABLE void logout();

Q_SIGNALS:
    void sessionChanged();
    void busyChanged();
    void serverHostChanged();
    void connectedChanged();
    void errorChanged();
    void loginFailed(const QString &reason);

private:
    void sendLogin(const QString &username, const QString &password, bool registerNew, const QString &displayName);
    void onLoginReply(const proto::Envelope &reply);

    NetworkClient  *m_network = nullptr;
    proto::UserInfo m_me;
    QString         m_serverHost;
    quint16         m_serverPort = 7400;
    QString         m_token;
    QString         m_httpBaseUrl;
    QString         m_error;
    bool            m_loggedIn = false;
    bool            m_busy = false;
};

} // namespace qm
