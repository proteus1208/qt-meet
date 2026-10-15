#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QSet>
#include <QUrl>

#include "media/MediaPlayer.h"
#include "messages.pb.h"

namespace qm {

class FileTransfer;
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
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(qm::MediaPlayer *player READ player CONSTANT)

public:
    ExplorerController(NetworkClient *network, SessionController *session, FileTransfer *transfers,
                       RemoteFsModel *files, MediaPlayer *player, QObject *parent = nullptr);

    QAbstractItemModel *files() const;
    QString             currentPath() const;
    bool                canGoUp() const;
    bool                busy() const;
    MediaPlayer        *player() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void openEntry(int row);           // folder: navigate, file: play
    Q_INVOKABLE void downloadEntry(int row);       // save hosted file to Downloads
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void openMedia(const QUrl &url);   // local file, http, rtsp

Q_SIGNALS:
    void currentPathChanged();
    void busyChanged();
    void playRequested(const QString &name);
    void downloadStarted(const QString &name);
    void downloadFinished(const QString &localPath);
    void errorOccurred(const QString &message);

private:
    void requestListing(const QString &path);
    void onListing(const proto::DirListRes &listing);

    NetworkClient     *m_network = nullptr;
    SessionController *m_session = nullptr;
    FileTransfer      *m_transfers = nullptr;
    RemoteFsModel     *m_files = nullptr;
    MediaPlayer       *m_player = nullptr;
    QString            m_currentPath;
    bool               m_busy = false;
    quint64            m_latestListSeq = 0;
    QSet<QString>      m_pendingDownloads;
};

} // namespace qm
