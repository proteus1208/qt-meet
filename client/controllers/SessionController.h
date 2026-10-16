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
    Q_PROPERTY(int transportMode READ transportMode NOTIFY sessionChanged)
    Q_PROPERTY(QString transportModeLabel READ transportModeLabel NOTIFY sessionChanged)
    Q_PROPERTY(int defaultCallBitrateKbps READ defaultCallBitrateKbps NOTIFY sessionChanged)
    Q_PROPERTY(int defaultMeetingBitrateKbps READ defaultMeetingBitrateKbps NOTIFY sessionChanged)

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
    quint16 rtcPortMin() const;
    quint16 rtcPortMax() const;
    int     transportMode() const;
    QString transportModeLabel() const;
    int     defaultCallBitrateKbps() const;
    int     defaultMeetingBitrateKbps() const;

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
    quint16         m_rtcPortMin = 40000;
    quint16         m_rtcPortMax = 40100;
    int             m_transportMode = 0; // 0 Direct, 1 Multicast
    int             m_defaultCallBitrateKbps = 1200;
    int             m_defaultMeetingBitrateKbps = 1200;
    bool            m_loggedIn = false;
    bool            m_busy = false;
};

} // namespace qm
