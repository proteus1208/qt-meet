#pragma once

#include <QObject>

#include <memory>

#include "common/AppConfig.h"

class QQmlApplicationEngine;

namespace qm {

class CallController;
class CallEngine;
class ChatController;
class ChatModel;
class ConversationModel;
class ExplorerController;
class FileTransfer;
class MediaPlayer;
class NetworkClient;
class ParticipantModel;
class RemoteFsModel;
class ResourceMonitor;
class SessionController;
class TransferModel;
class UserListModel;

/// Aim: client composition root. Builds core services, models and controllers, wires them,
/// and registers them with QML as singletons of module "QtMeet".
class ClientApp : public QObject
{
    Q_OBJECT

public:
    explicit ClientApp(const ClientSettings &settings, QObject *parent = nullptr);
    ~ClientApp() override;

    void registerQml();
    void start();
    void shutdown();

private:
    void buildComponents();

    ClientSettings m_settings;

    std::unique_ptr<NetworkClient>      m_network;
    std::unique_ptr<ResourceMonitor>    m_monitor;

    std::unique_ptr<UserListModel>      m_searchResults;
    std::unique_ptr<ConversationModel>  m_conversations;
    std::unique_ptr<ChatModel>          m_messages;
    std::unique_ptr<TransferModel>      m_transfers;
    std::unique_ptr<ParticipantModel>   m_participants;
    std::unique_ptr<RemoteFsModel>      m_remoteFiles;

    std::unique_ptr<FileTransfer>       m_fileTransfer;
    std::unique_ptr<CallEngine>         m_callEngine;
    std::unique_ptr<MediaPlayer>        m_player;

    std::unique_ptr<SessionController>  m_session;
    std::unique_ptr<ChatController>     m_chat;
    std::unique_ptr<CallController>     m_call;
    std::unique_ptr<ExplorerController> m_explorer;
};

} // namespace qm
