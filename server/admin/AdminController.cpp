#include "AdminController.h"

#include <QTimer>

#include "ServerApp.h"

namespace qm {

AdminController::AdminController(ServerApp *server, QObject *parent)
    : QObject(parent)
    , m_server(server)
{
    connect(m_server, &ServerApp::hostedFolderChanged, this, &AdminController::hostedFolderChanged);
    connect(m_server, &ServerApp::runningChanged, this, &AdminController::runningChanged);
    connect(m_server, &ServerApp::errorOccurred, this, &AdminController::errorOccurred);
}

QString AdminController::hostedFolder() const
{
    return m_server->hostedFolder();
}

bool AdminController::running() const
{
    return m_server->isRunning();
}

QStringList AdminController::onlineUsers() const
{
    return m_onlineUsers;
}

int AdminController::connectionCount() const
{
    return m_connectionCount;
}

QString AdminController::listenInfo() const
{
    return {};
}

void AdminController::start()
{
    m_server->start();
}

void AdminController::stop()
{
    m_server->stop();
}

void AdminController::chooseFolder(const QUrl &folder)
{
    m_server->setHostedFolder(folder.toLocalFile());
}

void AdminController::refreshStats()
{
}

} // namespace qm
