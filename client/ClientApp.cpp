#include "ClientApp.h"

#include <QtQml>

#include "common/ResourceMonitor.h"
#include "rtc/RtcConfig.h"
#include "controllers/CallController.h"
#include "controllers/ChatController.h"
#include "controllers/ExplorerController.h"
#include "controllers/SessionController.h"
#include "core/CallEngine.h"
#include "core/FileTransfer.h"
#include "core/NetworkClient.h"
#include "media/MediaPlayer.h"
#include "models/ChatModel.h"
#include "models/ConversationModel.h"
#include "models/ParticipantModel.h"
#include "models/RemoteFsModel.h"
#include "models/TransferModel.h"
#include "models/UserListModel.h"

namespace qm {

ClientApp::ClientApp(const ClientSettings &settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    buildComponents();
}

ClientApp::~ClientApp()
{
    shutdown();
}

void ClientApp::registerQml()
{
    qmlRegisterSingletonInstance("QtMeet", 1, 0, "Session", m_session.get());
    qmlRegisterSingletonInstance("QtMeet", 1, 0, "Chat", m_chat.get());
    qmlRegisterSingletonInstance("QtMeet", 1, 0, "Call", m_call.get());
    qmlRegisterSingletonInstance("QtMeet", 1, 0, "Explorer", m_explorer.get());
    qmlRegisterType<MediaPlayer>("QtMeet", 1, 0, "MediaPlayer");
}

void ClientApp::start()
{
    m_network->start();
}

void ClientApp::shutdown()
{
    if (m_network)
        m_network->shutdown();
}

void ClientApp::buildComponents()
{
    m_network = std::make_unique<NetworkClient>();
    m_monitor = std::make_unique<ResourceMonitor>();

    m_searchResults = std::make_unique<UserListModel>();
    m_conversations = std::make_unique<ConversationModel>();
    m_messages = std::make_unique<ChatModel>();
    m_transfers = std::make_unique<TransferModel>();
    m_participants = std::make_unique<ParticipantModel>();
    m_remoteFiles = std::make_unique<RemoteFsModel>();

    m_fileTransfer = std::make_unique<FileTransfer>(m_network.get(), m_transfers.get());
    m_callEngine = std::make_unique<CallEngine>(m_network.get(), m_monitor.get(), RtcConfig{});
    m_player = std::make_unique<MediaPlayer>();

    m_session = std::make_unique<SessionController>(m_network.get());
    m_session->setServerHost(m_settings.serverHost.isEmpty()
                                 ? QStringLiteral("127.0.0.1")
                                 : m_settings.serverHost);
    m_session->setServerPort(m_settings.tcpPort);
    m_chat = std::make_unique<ChatController>(m_network.get(), m_session.get(), m_fileTransfer.get(),
                                              m_searchResults.get(), m_conversations.get(),
                                              m_messages.get(), m_transfers.get());
    m_call = std::make_unique<CallController>(m_network.get(), m_session.get(), m_callEngine.get(),
                                              m_participants.get());
    m_explorer = std::make_unique<ExplorerController>(m_network.get(), m_session.get(),
                                                      m_remoteFiles.get(), m_player.get());

    connect(m_session.get(), &SessionController::sessionChanged, this, [this] {
        if (!m_session->loggedIn() || !m_callEngine)
            return;
        RtcConfig rtc;
        rtc.portRangeBegin = m_session->rtcPortMin();
        rtc.portRangeEnd = m_session->rtcPortMax();
        m_callEngine->setRtcConfig(rtc);
    });
}

} // namespace qm
