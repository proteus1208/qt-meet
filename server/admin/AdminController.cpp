#include "AdminController.h"

#include <algorithm>

#include <QSet>
#include <QTimer>
#include <QtGlobal>

#include "ServerApp.h"
#include "common/AppConfig.h"
#include "media/MediaSource.h"
#include "media/MediaTypes.h"
#include "net/ConnectionManager.h"
#include "net/SessionRegistry.h"
#include "services/ChannelService.h"
#include "storage/StorageManager.h"
#include "storage/StorageTypes.h"

namespace qm {

namespace {

QVariantMap sourceToVariant(const SourceSpec &spec)
{
    QVariantMap map;
    map.insert(QStringLiteral("kind"), int(spec.kind));
    map.insert(QStringLiteral("location"), spec.location);
    map.insert(QStringLiteral("format"), spec.format);
    map.insert(QStringLiteral("displayName"),
               spec.displayName.isEmpty() ? spec.location : spec.displayName);
    QVariantMap options;
    for (auto it = spec.options.begin(); it != spec.options.end(); ++it)
        options.insert(it.key(), it.value());
    map.insert(QStringLiteral("options"), options);
    return map;
}

SourceSpec sourceFromVariant(const QVariantMap &map)
{
    SourceSpec spec;
    spec.kind = SourceKind(map.value(QStringLiteral("kind")).toInt());
    spec.location = map.value(QStringLiteral("location")).toString();
    spec.format = map.value(QStringLiteral("format")).toString();
    spec.displayName = map.value(QStringLiteral("displayName")).toString();
    const QVariantMap options = map.value(QStringLiteral("options")).toMap();
    for (auto it = options.begin(); it != options.end(); ++it)
        spec.options.insert(it.key(), it.value().toString());
    return spec;
}

QVariantMap userRow(UserId id, const QString &name, const QString &username, bool online)
{
    QVariantMap row;
    row.insert(QStringLiteral("id"), qint64(id));
    row.insert(QStringLiteral("name"), name);
    row.insert(QStringLiteral("username"), username);
    row.insert(QStringLiteral("online"), online);
    return row;
}

} // namespace

AdminController::AdminController(ServerApp *server, const QString &configPath, QObject *parent)
    : QObject(parent)
    , m_server(server)
    , m_configPath(configPath)
{
    connect(m_server, &ServerApp::hostedFolderChanged, this, &AdminController::hostedFolderChanged);
    connect(m_server, &ServerApp::runningChanged, this, &AdminController::runningChanged);
    connect(m_server, &ServerApp::runningChanged, this, &AdminController::bindChannelSignals);
    connect(m_server, &ServerApp::runningChanged, this, &AdminController::bindSessionSignals);
    connect(m_server, &ServerApp::transportModeChanged, this, &AdminController::transportModeChanged);
    connect(m_server, &ServerApp::errorOccurred, this, &AdminController::errorOccurred);

    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(1000);
    connect(m_statsTimer, &QTimer::timeout, this, &AdminController::refreshStats);
    m_statsTimer->start();

    refreshSources();
}

QString AdminController::hostedFolder() const
{
    return m_server->hostedFolder();
}

bool AdminController::running() const
{
    return m_server->isRunning();
}

QVariantList AdminController::onlineUsers() const
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
    const bool httpUp = m_server->httpMediaListening();
    const QString mode = s.transportMode == TransportMode::Multicast ? QStringLiteral("Multicast")
                                                                     : QStringLiteral("Direct");
    return QStringLiteral("TCP %1 | HTTP %2%3 | UDP %4-%5 | %6")
        .arg(s.tcpPort)
        .arg(s.httpPort)
        .arg(httpUp ? QString() : QStringLiteral(" (DOWN)"))
        .arg(s.rtcPortMin)
        .arg(s.rtcPortMax)
        .arg(mode);
}

QVariantList AdminController::channels() const
{
    return m_channels;
}

QStringList AdminController::videoSourceLabels() const
{
    return m_videoLabels;
}

QStringList AdminController::audioSourceLabels() const
{
    return m_audioLabels;
}

int AdminController::transportMode() const
{
    return int(m_server->transportMode());
}

void AdminController::setTransportMode(int mode)
{
    m_server->setTransportMode(mode == 1 ? TransportMode::Multicast : TransportMode::Direct);
    persistSettings();
    Q_EMIT transportModeChanged();
    Q_EMIT runningChanged();
}

int AdminController::defaultChannelBitrateKbps() const
{
    return m_server->defaultChannelBitrateKbps();
}

void AdminController::setDefaultChannelBitrateKbps(int kbps)
{
    m_server->setDefaultChannelBitrateKbps(kbps);
    persistSettings();
    Q_EMIT transportModeChanged();
}

int AdminController::defaultCallBitrateKbps() const
{
    return m_server->defaultCallBitrateKbps();
}

void AdminController::setDefaultCallBitrateKbps(int kbps)
{
    m_server->setDefaultCallBitrateKbps(kbps);
    persistSettings();
    Q_EMIT transportModeChanged();
}

int AdminController::defaultMeetingBitrateKbps() const
{
    return m_server->defaultMeetingBitrateKbps();
}

void AdminController::setDefaultMeetingBitrateKbps(int kbps)
{
    m_server->setDefaultMeetingBitrateKbps(kbps);
    persistSettings();
    Q_EMIT transportModeChanged();
}

QStringList AdminController::transportModeLabels() const
{
    return {QStringLiteral("Direct (unicast SFU)"), QStringLiteral("Multicast (shared feed)")};
}

void AdminController::start()
{
    m_server->start();
    bindChannelSignals();
    bindSessionSignals();
    refreshChannels();
    refreshStats();
}

void AdminController::stop()
{
    m_server->stop();
    m_channels.clear();
    Q_EMIT channelsChanged();
    refreshStats();
}

void AdminController::chooseFolder(const QUrl &folder)
{
    m_server->setHostedFolder(folder.toLocalFile());
    persistSettings();
}

void AdminController::refreshSources()
{
    m_videoSources.clear();
    m_audioSources.clear();
    m_videoLabels.clear();
    m_audioLabels.clear();

    // Index 0 = None (video-only / audio-only channels).
    m_videoLabels.append(QStringLiteral("— None —"));
    m_audioLabels.append(QStringLiteral("— None —"));

    for (const SourceSpec &cam : MediaSource::enumerateCameras()) {
        m_videoSources.append(sourceToVariant(cam));
        m_videoLabels.append(cam.displayName.isEmpty() ? cam.location : cam.displayName);
    }
    for (const SourceSpec &screen : MediaSource::enumerateScreens()) {
        m_videoSources.append(sourceToVariant(screen));
        m_videoLabels.append(screen.displayName.isEmpty() ? QStringLiteral("Screen")
                                                          : screen.displayName);
    }

    // System output first so admins can pick speakers/loopback as the "mic" source.
    for (const SourceSpec &sys : MediaSource::enumerateSystemAudio()) {
        m_audioSources.append(sourceToVariant(sys));
        m_audioLabels.append(sys.displayName.isEmpty() ? sys.location : sys.displayName);
    }
    for (const SourceSpec &mic : MediaSource::enumerateMicrophones()) {
        bool dup = false;
        for (const QVariant &existing : m_audioSources) {
            if (existing.toMap().value(QStringLiteral("location")).toString() == mic.location) {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;
        m_audioSources.append(sourceToVariant(mic));
        m_audioLabels.append(mic.displayName.isEmpty() ? mic.location : mic.displayName);
    }

    Q_EMIT sourcesChanged();
}

void AdminController::refreshChannels()
{
    if (auto *svc = m_server->channels())
        m_channels = svc->listForAdmin();
    else
        m_channels.clear();
    Q_EMIT channelsChanged();
}

bool AdminController::addChannel(const QString &name, int videoIndex, int audioIndex,
                                 int bitrateKbps)
{
    auto *svc = m_server->channels();
    if (!svc) {
        Q_EMIT errorOccurred(QStringLiteral("Start the server before adding channels."));
        return false;
    }

    // Combo index 0 = None; 1..N map into m_*Sources.
    SourceSpec video;
    SourceSpec audio;
    if (videoIndex > 0) {
        const int src = videoIndex - 1;
        if (src < 0 || src >= m_videoSources.size()) {
            Q_EMIT errorOccurred(QStringLiteral("Pick a camera or screen source."));
            return false;
        }
        video = sourceFromVariant(m_videoSources.at(src).toMap());
    }
    if (audioIndex > 0) {
        const int src = audioIndex - 1;
        if (src < 0 || src >= m_audioSources.size()) {
            Q_EMIT errorOccurred(QStringLiteral("Pick a microphone or system-audio source."));
            return false;
        }
        audio = sourceFromVariant(m_audioSources.at(src).toMap());
    }
    if (video.location.isEmpty() && audio.location.isEmpty()) {
        Q_EMIT errorOccurred(QStringLiteral("Pick at least a video or audio source."));
        return false;
    }

    QString error;
    const QString id = svc->addChannel(name, video, audio, bitrateKbps, &error);
    if (id.isEmpty()) {
        Q_EMIT errorOccurred(error.isEmpty() ? QStringLiteral("Failed to add channel") : error);
        return false;
    }
    refreshChannels();
    return true;
}

bool AdminController::removeChannel(const QString &channelId)
{
    auto *svc = m_server->channels();
    if (!svc)
        return false;
    QString error;
    if (!svc->removeChannel(channelId, &error)) {
        Q_EMIT errorOccurred(error);
        return false;
    }
    refreshChannels();
    return true;
}

bool AdminController::startChannel(const QString &channelId)
{
    auto *svc = m_server->channels();
    if (!svc)
        return false;
    QString error;
    if (!svc->startChannel(channelId, &error)) {
        Q_EMIT errorOccurred(error);
        return false;
    }
    refreshChannels();
    return true;
}

bool AdminController::stopChannel(const QString &channelId)
{
    auto *svc = m_server->channels();
    if (!svc)
        return false;
    QString error;
    if (!svc->stopChannel(channelId, &error)) {
        Q_EMIT errorOccurred(error);
        return false;
    }
    refreshChannels();
    return true;
}

bool AdminController::setChannelBitrate(const QString &channelId, int bitrateKbps)
{
    auto *svc = m_server->channels();
    if (!svc)
        return false;
    QString error;
    if (!svc->setChannelBitrateKbps(channelId, bitrateKbps, &error)) {
        Q_EMIT errorOccurred(error);
        return false;
    }
    for (int i = 0; i < m_channels.size(); ++i) {
        QVariantMap row = m_channels.at(i).toMap();
        if (row.value(QStringLiteral("id")).toString() != channelId)
            continue;
        row.insert(QStringLiteral("bitrateKbps"), qBound(200, bitrateKbps, 8000));
        m_channels[i] = row;
        break;
    }
    return true;
}

void AdminController::refreshStats()
{
    QVariantList users;
    int conns = 0;
    SessionRegistry *sessions = m_server->sessions();
    StorageManager *storage = m_server->storage();

    if (storage) {
        const QList<UserRecord> found = storage->searchUsers(QString(), 500, 0).result();
        QSet<UserId> seen;
        for (const UserRecord &user : found) {
            const bool online = sessions && sessions->isOnline(user.id);
            users.append(userRow(user.id, user.displayName, user.username, online));
            seen.insert(user.id);
        }
        // Include anyone currently online who might not match the search (shouldn't happen).
        if (sessions) {
            for (UserId id : sessions->onlineUsers()) {
                if (seen.contains(id))
                    continue;
                users.append(userRow(id, sessions->displayName(id), sessions->username(id), true));
            }
        }
    } else if (sessions) {
        for (UserId id : sessions->onlineUsers())
            users.append(userRow(id, sessions->displayName(id), sessions->username(id), true));
    }

    if (auto *cm = m_server->connections())
        conns = cm->connectionCount();

    // Stable order: online first, then by display name.
    std::sort(users.begin(), users.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap ma = a.toMap();
        const QVariantMap mb = b.toMap();
        const bool oa = ma.value(QStringLiteral("online")).toBool();
        const bool ob = mb.value(QStringLiteral("online")).toBool();
        if (oa != ob)
            return oa && !ob;
        return ma.value(QStringLiteral("name")).toString().compare(
                   mb.value(QStringLiteral("name")).toString(), Qt::CaseInsensitive)
               < 0;
    });

    if (users != m_onlineUsers || conns != m_connectionCount) {
        m_onlineUsers = users;
        m_connectionCount = conns;
        Q_EMIT statsChanged();
    }
}

void AdminController::bindChannelSignals()
{
    if (auto *svc = m_server->channels()) {
        connect(svc, &ChannelService::channelsChanged, this, &AdminController::refreshChannels,
                Qt::UniqueConnection);
        refreshChannels();
    }
}

void AdminController::bindSessionSignals()
{
    if (auto *sessions = m_server->sessions()) {
        connect(sessions, &SessionRegistry::userOnline, this, &AdminController::refreshStats,
                Qt::UniqueConnection);
        connect(sessions, &SessionRegistry::userOffline, this, &AdminController::refreshStats,
                Qt::UniqueConnection);
        refreshStats();
    }
}

void AdminController::persistSettings()
{
    if (m_configPath.isEmpty())
        return;
    AppConfig cfg;
    cfg.load(m_configPath);
    ServerSettings s = m_server->settings();
    // Keep non-runtime fields from disk if present.
    const ServerSettings previous = cfg.server();
    if (s.certFile.isEmpty())
        s.certFile = previous.certFile;
    if (s.keyFile.isEmpty())
        s.keyFile = previous.keyFile;
    if (s.dataDir.isEmpty())
        s.dataDir = previous.dataDir;
    // Always persist the current hosted folder (including clearing it).
    s.hostedFolder = m_server->hostedFolder();
    // Tray hotkey is owned by DesktopShell / ini — don't clobber it here.
    if (!previous.showHotkey.isEmpty())
        s.showHotkey = previous.showHotkey;
    cfg.setServer(s);
    cfg.save(m_configPath);
}

} // namespace qm
