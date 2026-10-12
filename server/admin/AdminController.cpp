#include "AdminController.h"

#include <QTimer>

#include "ServerApp.h"
#include "net/ConnectionManager.h"
#include "net/SessionRegistry.h"
#include "storage/InMemoryUsers.h"

namespace qm {

AdminController::AdminController(ServerApp *server, QObject *parent)
    : QObject(parent)
    , m_server(server)
{
    connect(m_server, &ServerApp::hostedFolderChanged, this, &AdminController::hostedFolderChanged);
    connect(m_server, &ServerApp::runningChanged, this, &AdminController::runningChanged);
    connect(m_server, &ServerApp::errorOccurred, this, &AdminController::errorOccurred);

    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(1000);
    connect(m_statsTimer, &QTimer::timeout, this, &AdminController::refreshStats);
    m_statsTimer->start();
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
    if (!m_server->isRunning())
        return QStringLiteral("Stopped");
    const auto &s = m_server->settings();
    return QStringLiteral("TCP %1 | HTTP %2 | UDP %3-%4")
        .arg(s.tcpPort)
        .arg(s.httpPort)
        .arg(s.rtcPortMin)
        .arg(s.rtcPortMax);
}

void AdminController::start()
{
    m_server->start();
    refreshStats();
}

void AdminController::stop()
{
    m_server->stop();
    refreshStats();
}

void AdminController::chooseFolder(const QUrl &folder)
{
    m_server->setHostedFolder(folder.toLocalFile());
}

void AdminController::refreshStats()
{
    QStringList names;
    int conns = 0;
    if (auto *sessions = m_server->sessions()) {
        for (UserId id : sessions->onlineUsers())
            names.append(InMemoryUsers::instance().displayName(id));
    }
    if (auto *cm = m_server->connections())
        conns = cm->connectionCount();

    names.sort(Qt::CaseInsensitive);
    if (names != m_onlineUsers || conns != m_connectionCount) {
        m_onlineUsers = names;
        m_connectionCount = conns;
        Q_EMIT statsChanged();
    }
    Q_EMIT runningChanged();
}

} // namespace qm
