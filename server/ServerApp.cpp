#include "ServerApp.h"

#include "dispatch/Dispatcher.h"
#include "net/ConnectionManager.h"
#include "net/SessionRegistry.h"
#include "services/AuthService.h"
#include "services/ChatService.h"
#include "services/FileService.h"
#include "services/FolderService.h"
#include "services/MeetingService.h"
#include "services/UserService.h"
#include "storage/StorageManager.h"
#include "streaming/HttpMediaServer.h"
#include "streaming/Sfu.h"

namespace qm {

ServerApp::ServerApp(const ServerSettings &settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

ServerApp::~ServerApp()
{
    stop();
}

bool ServerApp::start()
{
    return false;
}

void ServerApp::stop()
{
}

bool ServerApp::isRunning() const
{
    return m_running;
}

void ServerApp::setHostedFolder(const QString &absolutePath)
{
    if (m_settings.hostedFolder == absolutePath)
        return;
    m_settings.hostedFolder = absolutePath;
    Q_EMIT hostedFolderChanged();
}

QString ServerApp::hostedFolder() const
{
    return m_settings.hostedFolder;
}

SessionRegistry *ServerApp::sessions() const
{
    return m_sessions.get();
}

ConnectionManager *ServerApp::connections() const
{
    return m_connections.get();
}

const ServerSettings &ServerApp::settings() const
{
    return m_settings;
}

void ServerApp::buildComponents()
{
}

void ServerApp::registerHandlers()
{
}

QSslConfiguration ServerApp::loadSsl() const
{
    return {};
}

} // namespace qm
