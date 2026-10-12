#include "ExplorerController.h"

#include "controllers/SessionController.h"
#include "core/NetworkClient.h"
#include "models/RemoteFsModel.h"

namespace qm {

ExplorerController::ExplorerController(NetworkClient *network, SessionController *session,
                                       RemoteFsModel *files, MediaPlayer *player, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_session(session)
    , m_files(files)
    , m_player(player)
{
}

QAbstractItemModel *ExplorerController::files() const
{
    return m_files;
}

QString ExplorerController::currentPath() const
{
    return m_currentPath;
}

bool ExplorerController::canGoUp() const
{
    return !m_currentPath.isEmpty() && m_currentPath != QLatin1String("/");
}

MediaPlayer *ExplorerController::player() const
{
    return m_player;
}

void ExplorerController::refresh()
{
    openPath(m_currentPath);
}

void ExplorerController::openPath(const QString &path)
{
    Q_UNUSED(path)
}

void ExplorerController::openEntry(int row)
{
    Q_UNUSED(row)
}

void ExplorerController::goUp()
{
}

void ExplorerController::onListing(const proto::DirListRes &listing)
{
    Q_UNUSED(listing)
}

} // namespace qm
