#include "AdminController.h"

#include <QTimer>

#include "ServerApp.h"
#include "common/AppConfig.h"
#include "media/MediaSource.h"
#include "media/MediaTypes.h"
#include "net/ConnectionManager.h"
#include "net/SessionRegistry.h"
#include "services/ChannelService.h"

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

} // namespace

AdminController::AdminController(ServerApp *server, const QString &configPath, QObject *parent)
    : QObject(parent)
    , m_server(server)
    , m_configPath(configPath)
{
    connect(m_server, &ServerApp::hostedFolderChanged, this, &AdminController::hostedFolderChanged);
    connect(m_server, &ServerApp::runningChanged, this, &AdminController::runningChanged);
    connect(m_server, &ServerApp::runningChanged, this, &AdminController::bindChannelSignals);
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

QStringList AdminController::transportModeLabels() const
{
    return {QStringLiteral("Direct (unicast SFU)"), QStringLiteral("Multicast (shared feed)")};
}

void AdminController::start()
{
    m_server->start();
    bindChannelSignals();
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

    for (const SourceSpec &cam : MediaSource::enumerateCameras()) {
        m_videoSources.append(sourceToVariant(cam));
        m_videoLabels.append(cam.displayName.isEmpty() ? cam.location : cam.displayName);
    }
    for (const SourceSpec &screen : MediaSource::enumerateScreens()) {
        m_videoSources.append(sourceToVariant(screen));
        m_videoLabels.append(screen.displayName.isEmpty() ? QStringLiteral("Screen")
                                                          : screen.displayName);
    }

    for (const SourceSpec &mic : MediaSource::enumerateMicrophones()) {
        m_audioSources.append(sourceToVariant(mic));
        m_audioLabels.append(mic.displayName.isEmpty() ? mic.location : mic.displayName);
    }
    for (const SourceSpec &sys : MediaSource::enumerateSystemAudio()) {
        bool dup = false;
        for (const QVariant &existing : m_audioSources) {
            if (existing.toMap().value(QStringLiteral("location")).toString() == sys.location) {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;
        m_audioSources.append(sourceToVariant(sys));
        m_audioLabels.append(sys.displayName.isEmpty() ? sys.location : sys.displayName);
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

bool AdminController::addChannel(const QString &name, int videoIndex, int audioIndex)
{
    auto *svc = m_server->channels();
    if (!svc) {
        Q_EMIT errorOccurred(QStringLiteral("Start the server before adding channels."));
        return false;
    }
    if (videoIndex < 0 || videoIndex >= m_videoSources.size()) {
        Q_EMIT errorOccurred(QStringLiteral("Pick a camera or screen source."));
        return false;
    }
    if (audioIndex < 0 || audioIndex >= m_audioSources.size()) {
        Q_EMIT errorOccurred(QStringLiteral("Pick a microphone or system-audio source."));
        return false;
    }

    QString error;
    const QString id =
        svc->addChannel(name, sourceFromVariant(m_videoSources.at(videoIndex).toMap()),
                        sourceFromVariant(m_audioSources.at(audioIndex).toMap()), &error);
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

void AdminController::refreshStats()
{
    QStringList names;
    int conns = 0;
    if (auto *sessions = m_server->sessions()) {
        for (UserId id : sessions->onlineUsers())
            names.append(sessions->displayName(id));
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

void AdminController::bindChannelSignals()
{
    if (auto *svc = m_server->channels()) {
        connect(svc, &ChannelService::channelsChanged, this, &AdminController::refreshChannels,
                Qt::UniqueConnection);
        refreshChannels();
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
    cfg.setServer(s);
    cfg.save(m_configPath);
}

} // namespace qm
