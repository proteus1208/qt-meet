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
    Q_PROPERTY(QVariantList onlineUsers READ onlineUsers NOTIFY statsChanged)
    Q_PROPERTY(int connectionCount READ connectionCount NOTIFY statsChanged)
    Q_PROPERTY(QString listenInfo READ listenInfo NOTIFY runningChanged)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY channelsChanged)
    Q_PROPERTY(QStringList videoSourceLabels READ videoSourceLabels NOTIFY sourcesChanged)
    Q_PROPERTY(QStringList audioSourceLabels READ audioSourceLabels NOTIFY sourcesChanged)
    Q_PROPERTY(int transportMode READ transportMode WRITE setTransportMode NOTIFY transportModeChanged)
    Q_PROPERTY(int defaultChannelBitrateKbps READ defaultChannelBitrateKbps
               WRITE setDefaultChannelBitrateKbps NOTIFY transportModeChanged)
    Q_PROPERTY(int defaultCallBitrateKbps READ defaultCallBitrateKbps
               WRITE setDefaultCallBitrateKbps NOTIFY transportModeChanged)
    Q_PROPERTY(int defaultMeetingBitrateKbps READ defaultMeetingBitrateKbps
               WRITE setDefaultMeetingBitrateKbps NOTIFY transportModeChanged)
    Q_PROPERTY(QStringList transportModeLabels READ transportModeLabels CONSTANT)

public:
    AdminController(ServerApp *server, const QString &configPath, QObject *parent = nullptr);

    QString      hostedFolder() const;
    bool         running() const;
    QVariantList onlineUsers() const;
    int          connectionCount() const;
    QString      listenInfo() const;
    QVariantList channels() const;
    QStringList  videoSourceLabels() const;
    QStringList  audioSourceLabels() const;
    int          transportMode() const;
    void         setTransportMode(int mode);
    int          defaultChannelBitrateKbps() const;
    void         setDefaultChannelBitrateKbps(int kbps);
    int          defaultCallBitrateKbps() const;
    void         setDefaultCallBitrateKbps(int kbps);
    int          defaultMeetingBitrateKbps() const;
    void         setDefaultMeetingBitrateKbps(int kbps);
    QStringList  transportModeLabels() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void chooseFolder(const QUrl &folder);

    Q_INVOKABLE void refreshSources();
    Q_INVOKABLE void refreshChannels();
    /// videoIndex / audioIndex: 0 = None, 1..N = device from refreshSources lists.
    Q_INVOKABLE bool addChannel(const QString &name, int videoIndex, int audioIndex,
                                int bitrateKbps = 0);
    Q_INVOKABLE bool updateChannel(const QString &channelId, const QString &name, int videoIndex,
                                   int audioIndex, int bitrateKbps = 0);
    Q_INVOKABLE bool removeChannel(const QString &channelId);
    Q_INVOKABLE bool startChannel(const QString &channelId);
    Q_INVOKABLE bool stopChannel(const QString &channelId);
    Q_INVOKABLE bool setChannelBitrate(const QString &channelId, int bitrateKbps);
    /// Match saved source location to combo index (0 = None).
    Q_INVOKABLE int videoSourceIndexForLocation(const QString &location) const;
    Q_INVOKABLE int audioSourceIndexForLocation(const QString &location) const;

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
    void bindSessionSignals();
    void persistSettings();

    ServerApp   *m_server = nullptr;
    QString      m_configPath;
    QTimer      *m_statsTimer = nullptr;
    QVariantList m_onlineUsers;
    int          m_connectionCount = 0;
    QVariantList m_channels;
    QStringList  m_videoLabels;
    QStringList  m_audioLabels;
    QVariantList m_videoSources;
    QVariantList m_audioSources;
};

} // namespace qm
