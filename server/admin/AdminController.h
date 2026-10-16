#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

class QTimer;

namespace qm {

class ServerApp;

/// Aim: QML facade of the server admin window: pick the hosted folder, start / stop,
/// choose Direct vs Multicast delivery, manage IPTV live channels.
class AdminController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString hostedFolder READ hostedFolder NOTIFY hostedFolderChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QStringList onlineUsers READ onlineUsers NOTIFY statsChanged)
    Q_PROPERTY(int connectionCount READ connectionCount NOTIFY statsChanged)
    Q_PROPERTY(QString listenInfo READ listenInfo NOTIFY runningChanged)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY channelsChanged)
    Q_PROPERTY(QStringList videoSourceLabels READ videoSourceLabels NOTIFY sourcesChanged)
    Q_PROPERTY(QStringList audioSourceLabels READ audioSourceLabels NOTIFY sourcesChanged)
    Q_PROPERTY(int transportMode READ transportMode WRITE setTransportMode NOTIFY transportModeChanged)
    Q_PROPERTY(int defaultChannelBitrateKbps READ defaultChannelBitrateKbps
               WRITE setDefaultChannelBitrateKbps NOTIFY transportModeChanged)
    Q_PROPERTY(QStringList transportModeLabels READ transportModeLabels CONSTANT)

public:
    AdminController(ServerApp *server, const QString &configPath, QObject *parent = nullptr);

    QString      hostedFolder() const;
    bool         running() const;
    QStringList  onlineUsers() const;
    int          connectionCount() const;
    QString      listenInfo() const;
    QVariantList channels() const;
    QStringList  videoSourceLabels() const;
    QStringList  audioSourceLabels() const;
    int          transportMode() const;
    void         setTransportMode(int mode);
    int          defaultChannelBitrateKbps() const;
    void         setDefaultChannelBitrateKbps(int kbps);
    QStringList  transportModeLabels() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void chooseFolder(const QUrl &folder);

    Q_INVOKABLE void refreshSources();
    Q_INVOKABLE void refreshChannels();
    Q_INVOKABLE bool addChannel(const QString &name, int videoIndex, int audioIndex);
    Q_INVOKABLE bool removeChannel(const QString &channelId);
    Q_INVOKABLE bool startChannel(const QString &channelId);
    Q_INVOKABLE bool stopChannel(const QString &channelId);

Q_SIGNALS:
    void hostedFolderChanged();
    void runningChanged();
    void statsChanged();
    void channelsChanged();
    void sourcesChanged();
    void transportModeChanged();
    void errorOccurred(const QString &message);

private:
    void refreshStats();
    void bindChannelSignals();
    void persistSettings();

    ServerApp   *m_server = nullptr;
    QString      m_configPath;
    QTimer      *m_statsTimer = nullptr;
    QStringList  m_onlineUsers;
    int          m_connectionCount = 0;
    QVariantList m_channels;
    QStringList  m_videoLabels;
    QStringList  m_audioLabels;
    QVariantList m_videoSources;
    QVariantList m_audioSources;
};

} // namespace qm
