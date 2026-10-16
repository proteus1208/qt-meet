#include "ServerApp.h"

#include <QDir>
#include <QFile>
#include <QSslCertificate>
#include <QSslKey>
#include <QtGlobal>
#include <QThread>

#include "common/Ids.h"
#include "common/Logger.h"
#include "dispatch/Dispatcher.h"
#include "net/ConnectionManager.h"
#include "net/SessionRegistry.h"
#include "services/AuthService.h"
#include "services/ChannelService.h"
#include "services/ChatService.h"
#include "services/FileService.h"
#include "services/FolderService.h"
#include "services/MeetingService.h"
#include "services/UserService.h"
#include "storage/StorageManager.h"
#include "rtc/RtcConfig.h"
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
    if (m_running)
        return true;

    if (m_settings.dataDir.isEmpty())
        m_settings.dataDir = QDir::homePath() + QStringLiteral("/.qtmeet/data");
    QDir().mkpath(m_settings.dataDir);
    QDir().mkpath(m_settings.dataDir + QStringLiteral("/blobs"));

    const QSslConfiguration ssl = loadSsl();
    if (ssl.localCertificate().isNull() || ssl.privateKey().isNull()) {
        Q_EMIT errorOccurred(QStringLiteral("TLS certificate/key missing. Expected:\n%1\n%2")
                                 .arg(m_settings.certFile, m_settings.keyFile));
        return false;
    }

    buildComponents();
    if (!m_storage->start().result()) {
        Q_EMIT errorOccurred(QStringLiteral("Cannot open database %1")
                                 .arg(QDir::toNativeSeparators(databasePath())));
        stop();
        return false;
    }
    registerHandlers();

    const int netThreads = m_settings.netThreads > 0
                               ? m_settings.netThreads
                               : qMax(1, QThread::idealThreadCount() / 2);

    m_connections = std::make_unique<ConnectionManager>(netThreads, ssl, m_sessions.get());
    connect(m_connections.get(), &ConnectionManager::envelopeReceived,
            m_dispatcher.get(), &Dispatcher::dispatch);
    connect(m_connections.get(), &ConnectionManager::connectionOpened,
            m_dispatcher.get(), &Dispatcher::onConnectionOpened);
    connect(m_connections.get(), &ConnectionManager::connectionClosed,
            m_dispatcher.get(), &Dispatcher::onConnectionClosed);

    if (!m_connections->start(m_settings.bindAddress, m_settings.tcpPort)) {
        Q_EMIT errorOccurred(QStringLiteral("Failed to listen on TCP %1").arg(m_settings.tcpPort));
        stop();
        return false;
    }

    connect(m_connections.get(), &ConnectionManager::connectionClosed, this, [this](ConnId conn) {
        if (m_files)
            m_files->abortUploadsOf(conn);
    });

    if (!m_http->start(m_settings.bindAddress, m_settings.httpPort)) {
        const QString msg =
            QStringLiteral(
                "HTTP media server did not start on port %1. "
                "Video/audio playback and attachment downloads will fail. "
                "Install the Qt HTTP Server module and rebuild, or free the port.")
                .arg(m_settings.httpPort);
        qCWarning(lcApp) << msg;
        Q_EMIT errorOccurred(msg);
    }

    m_running = true;
    Q_EMIT runningChanged();
    qCInfo(lcApp) << "QtMeetServer started on port" << m_settings.tcpPort;
    return true;
}

void ServerApp::stop()
{
    if (m_connections)
        m_connections->stop();
    if (m_dispatcher)
        m_dispatcher->shutdown();
    if (m_http)
        m_http->stop();

    m_channels.reset();
    m_meetings.reset();
    m_folders.reset();
    m_files.reset();
    m_chat.reset();
    m_users.reset();
    m_auth.reset();
    m_sfu.reset();
    m_http.reset();
    m_connections.reset();
    m_dispatcher.reset();
    m_sessions.reset();
    m_storage.reset();

    if (m_running) {
        m_running = false;
        Q_EMIT runningChanged();
    }
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
    if (m_folders)
        m_folders->setRoot(absolutePath);
    if (m_http)
        m_http->setHostedRoot(absolutePath);
    Q_EMIT hostedFolderChanged();
}

QString ServerApp::hostedFolder() const
{
    return m_settings.hostedFolder;
}

void ServerApp::setTransportMode(TransportMode mode)
{
    if (m_settings.transportMode == mode)
        return;
    m_settings.transportMode = mode;
    if (m_auth)
        m_auth->setSettings(m_settings);
    if (m_channels)
        m_channels->setTransportMode(mode);
    Q_EMIT transportModeChanged();
}

TransportMode ServerApp::transportMode() const
{
    return m_settings.transportMode;
}

void ServerApp::setDefaultChannelBitrateKbps(int kbps)
{
    const int clamped = qBound(200, kbps, 8000);
    if (m_settings.defaultChannelBitrateKbps == clamped)
        return;
    m_settings.defaultChannelBitrateKbps = clamped;
    if (m_auth)
        m_auth->setSettings(m_settings);
    if (m_channels)
        m_channels->setDefaultBitrateKbps(clamped);
}

int ServerApp::defaultChannelBitrateKbps() const
{
    return m_settings.defaultChannelBitrateKbps;
}

void ServerApp::setDefaultCallBitrateKbps(int kbps)
{
    const int clamped = qBound(200, kbps, 8000);
    if (m_settings.defaultCallBitrateKbps == clamped)
        return;
    m_settings.defaultCallBitrateKbps = clamped;
    if (m_auth)
        m_auth->setSettings(m_settings);
    Q_EMIT transportModeChanged();
}

int ServerApp::defaultCallBitrateKbps() const
{
    return m_settings.defaultCallBitrateKbps;
}

void ServerApp::setDefaultMeetingBitrateKbps(int kbps)
{
    const int clamped = qBound(200, kbps, 8000);
    if (m_settings.defaultMeetingBitrateKbps == clamped)
        return;
    m_settings.defaultMeetingBitrateKbps = clamped;
    if (m_auth)
        m_auth->setSettings(m_settings);
    Q_EMIT transportModeChanged();
}

int ServerApp::defaultMeetingBitrateKbps() const
{
    return m_settings.defaultMeetingBitrateKbps;
}

bool ServerApp::httpMediaListening() const
{
    return m_http && m_http->isListening();
}

SessionRegistry *ServerApp::sessions() const
{
    return m_sessions.get();
}

ConnectionManager *ServerApp::connections() const
{
    return m_connections.get();
}

ChannelService *ServerApp::channels() const
{
    return m_channels.get();
}

StorageManager *ServerApp::storage() const
{
    return m_storage.get();
}

const ServerSettings &ServerApp::settings() const
{
    return m_settings;
}

QString ServerApp::databasePath() const
{
    return m_settings.dataDir + QStringLiteral("/qtmeet.db");
}

void ServerApp::buildComponents()
{
    m_storage = std::make_unique<StorageManager>(databasePath());
    m_sessions = std::make_unique<SessionRegistry>();
    m_http = std::make_unique<HttpMediaServer>(m_storage.get());
    m_http->setBlobDir(m_settings.dataDir + QStringLiteral("/blobs"));
    m_http->setHostedRoot(m_settings.hostedFolder);

    const int handlers = m_settings.handlerThreads > 0
                             ? m_settings.handlerThreads
                             : qMax(2, QThread::idealThreadCount());
    m_dispatcher = std::make_unique<Dispatcher>(m_sessions.get(), handlers);

    m_auth = std::make_unique<AuthService>(m_storage.get(), m_sessions.get(), m_http.get(), m_settings);
    m_users = std::make_unique<UserService>(m_storage.get(), m_sessions.get());
    m_chat = std::make_unique<ChatService>(m_storage.get(), m_sessions.get());
    m_files = std::make_unique<FileService>(m_storage.get(), m_sessions.get(),
                                            m_settings.dataDir + QStringLiteral("/blobs"));
    m_folders = std::make_unique<FolderService>(m_sessions.get(), m_http.get());
    m_folders->setRoot(m_settings.hostedFolder);

    RtcConfig rtc;
    rtc.portRangeBegin = m_settings.rtcPortMin;
    rtc.portRangeEnd = m_settings.rtcPortMax;
    m_sfu = std::make_unique<Sfu>(rtc);
    m_meetings = std::make_unique<MeetingService>(m_storage.get(), m_sessions.get(), m_sfu.get());
    m_channels = std::make_unique<ChannelService>(
        m_sessions.get(), m_sfu.get(), m_settings.dataDir + QStringLiteral("/channels.json"));
    m_channels->setTransportMode(m_settings.transportMode);
    m_channels->setDefaultBitrateKbps(m_settings.defaultChannelBitrateKbps);

    connect(m_sessions.get(), &SessionRegistry::userOnline, m_users.get(), &UserService::onUserOnline);
    connect(m_sessions.get(), &SessionRegistry::userOffline, m_users.get(), &UserService::onUserOffline);
    connect(m_sessions.get(), &SessionRegistry::userOffline, m_meetings.get(),
            &MeetingService::onUserOffline);
    connect(m_sessions.get(), &SessionRegistry::userOffline, m_channels.get(),
            &ChannelService::onUserOffline);
}

void ServerApp::registerHandlers()
{
    m_dispatcher->registerHandler(proto::Envelope::kLoginReq, m_auth.get());
    m_dispatcher->registerHandler(proto::Envelope::kLogout, m_auth.get());
    m_dispatcher->registerHandler(proto::Envelope::kUserSearchReq, m_users.get());
    m_dispatcher->registerHandler(proto::Envelope::kChatSend, m_chat.get());
    m_dispatcher->registerHandler(proto::Envelope::kChatHistoryReq, m_chat.get());
    m_dispatcher->registerHandler(proto::Envelope::kFileOffer, m_files.get());
    m_dispatcher->registerHandler(proto::Envelope::kFileChunk, m_files.get());
    m_dispatcher->registerHandler(proto::Envelope::kDirListReq, m_folders.get());
    m_dispatcher->registerHandler(proto::Envelope::kCallInvite, m_meetings.get());
    m_dispatcher->registerHandler(proto::Envelope::kCallAnswer, m_meetings.get());
    m_dispatcher->registerHandler(proto::Envelope::kCallHangup, m_meetings.get());
    m_dispatcher->registerHandler(proto::Envelope::kRtcSignal, m_meetings.get());
    m_dispatcher->registerHandler(proto::Envelope::kMediaState, m_meetings.get());
    m_dispatcher->registerHandler(proto::Envelope::kMeetingCreate, m_meetings.get());
    m_dispatcher->registerHandler(proto::Envelope::kMeetingJoin, m_meetings.get());
    m_dispatcher->registerHandler(proto::Envelope::kMeetingLeave, m_meetings.get());
    m_dispatcher->registerHandler(proto::Envelope::kChannelListReq, m_channels.get());
    m_dispatcher->registerHandler(proto::Envelope::kChannelJoin, m_channels.get());
    m_dispatcher->registerHandler(proto::Envelope::kChannelLeave, m_channels.get());
    m_dispatcher->registerHandler(proto::Envelope::kChannelBandwidth, m_channels.get());
}

QSslConfiguration ServerApp::loadSsl() const
{
    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();

    QFile certFile(m_settings.certFile);
    QFile keyFile(m_settings.keyFile);
    if (!certFile.open(QIODevice::ReadOnly) || !keyFile.open(QIODevice::ReadOnly)) {
        qCWarning(lcApp) << "Cannot open TLS files" << m_settings.certFile << m_settings.keyFile;
        return conf;
    }

    const QSslCertificate cert(&certFile, QSsl::Pem);
    const QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
    conf.setLocalCertificate(cert);
    conf.setPrivateKey(key);
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);
    return conf;
}

} // namespace qm
