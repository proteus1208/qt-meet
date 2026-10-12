#pragma once

#include <QAbstractItemModel>
#include <QObject>

#include "media/MediaPlayer.h"
#include "messages.pb.h"

namespace qm {

class NetworkClient;
class RemoteFsModel;
class SessionController;

/// Aim: browse the server's hosted folder and open a file in the shared MediaPlayer.
class ExplorerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *files READ files CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY currentPathChanged)
    Q_PROPERTY(qm::MediaPlayer *player READ player CONSTANT)

public:
    ExplorerController(NetworkClient *network, SessionController *session,
                       RemoteFsModel *files, MediaPlayer *player, QObject *parent = nullptr);

    QAbstractItemModel *files() const;
    QString             currentPath() const;
    bool                canGoUp() const;
    MediaPlayer        *player() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void openEntry(int row);           // folder: navigate, file: play
    Q_INVOKABLE void goUp();

Q_SIGNALS:
    void currentPathChanged();
    void playRequested(const QString &name);
    void errorOccurred(const QString &message);

private:
    void onListing(const proto::DirListRes &listing);

    NetworkClient     *m_network = nullptr;
    SessionController *m_session = nullptr;
    RemoteFsModel     *m_files = nullptr;
    MediaPlayer       *m_player = nullptr;
    QString            m_currentPath;
};

} // namespace qm
