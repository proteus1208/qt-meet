#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>

class QTimer;

namespace qm {

class ServerApp;

/// Aim: QML facade of the server admin window: pick the hosted folder, start / stop,
/// show online users and connection count.
class AdminController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString hostedFolder READ hostedFolder NOTIFY hostedFolderChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QStringList onlineUsers READ onlineUsers NOTIFY statsChanged)
    Q_PROPERTY(int connectionCount READ connectionCount NOTIFY statsChanged)
    Q_PROPERTY(QString listenInfo READ listenInfo NOTIFY runningChanged)

public:
    explicit AdminController(ServerApp *server, QObject *parent = nullptr);

    QString     hostedFolder() const;
    bool        running() const;
    QStringList onlineUsers() const;
    int         connectionCount() const;
    QString     listenInfo() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void chooseFolder(const QUrl &folder);

Q_SIGNALS:
    void hostedFolderChanged();
    void runningChanged();
    void statsChanged();
    void errorOccurred(const QString &message);

private:
    void refreshStats();

    ServerApp  *m_server = nullptr;
    QTimer     *m_statsTimer = nullptr;
    QStringList m_onlineUsers;
    int         m_connectionCount = 0;
};

} // namespace qm
