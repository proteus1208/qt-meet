#pragma once

#include <QObject>
#include <QSslConfiguration>

#include <memory>

#include "common/AppConfig.h"

namespace qm {

class AuthService;
class ChannelService;
class ChatService;
class ConnectionManager;
class Dispatcher;
class FileService;
class FolderService;
class HttpMediaServer;
class MeetingService;
class SessionRegistry;
class Sfu;
class StorageManager;
class UserService;

/// Aim: server composition root. Builds every component, wires signals between them,
/// owns their threads and controls start / stop order.
class ServerApp : public QObject
{
    Q_OBJECT

public:
    explicit ServerApp(const ServerSettings &settings, QObject *parent = nullptr);
    ~ServerApp() override;

    bool start();
    void stop();
    bool isRunning() const;

    void    setHostedFolder(const QString &absolutePath);
    QString hostedFolder() const;
    bool    httpMediaListening() const;

    void          setTransportMode(TransportMode mode);
    TransportMode transportMode() const;
    void          setDefaultChannelBitrateKbps(int kbps);
    int           defaultChannelBitrateKbps() const;

    SessionRegistry   *sessions() const;
    ConnectionManager *connections() const;
    ChannelService    *channels() const;
    const ServerSettings &settings() const;

Q_SIGNALS:
    void runningChanged();
    void hostedFolderChanged();
    void transportModeChanged();
    void errorOccurred(const QString &message);

private:
    void              buildComponents();
    void              registerHandlers();
    QSslConfiguration loadSsl() const;
    QString           databasePath() const;

    ServerSettings m_settings;
    bool           m_running = false;

    std::unique_ptr<StorageManager>    m_storage;
    std::unique_ptr<SessionRegistry>   m_sessions;
    std::unique_ptr<Dispatcher>        m_dispatcher;
    std::unique_ptr<ConnectionManager> m_connections;
    std::unique_ptr<HttpMediaServer>   m_http;
    std::unique_ptr<Sfu>               m_sfu;

    std::unique_ptr<AuthService>       m_auth;
    std::unique_ptr<UserService>       m_users;
    std::unique_ptr<ChatService>       m_chat;
    std::unique_ptr<FileService>       m_files;
    std::unique_ptr<MeetingService>    m_meetings;
    std::unique_ptr<FolderService>     m_folders;
    std::unique_ptr<ChannelService>    m_channels;
};

} // namespace qm
