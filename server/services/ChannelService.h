#pragma once

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>

#include <memory>

#include "common/AppConfig.h"
#include "dispatch/IHandler.h"
#include "media/MediaTypes.h"

namespace qm {

class ChannelPublisher;
class SessionRegistry;
class Sfu;

/// Aim: IPTV-style live channels. Admin configures name + camera/screen + mic/system-audio;
/// server captures and publishes into an SFU room. Clients list / join / leave to watch.
class ChannelService : public QObject, public IHandler
{
    Q_OBJECT

public:
    ChannelService(SessionRegistry *sessions, Sfu *sfu, const QString &storePath,
                   QObject *parent = nullptr);
    ~ChannelService() override;

    void handle(const RequestContext &context, const proto::Envelope &envelope) override;

    void setTransportMode(TransportMode mode);
    void setDefaultBitrateKbps(int kbps);

    /// Admin APIs (called from AdminController on the GUI thread).
    QVariantList listForAdmin() const;
    QString      addChannel(const QString &name, const SourceSpec &video, const SourceSpec &audio,
                            int bitrateKbps = 0, QString *error = nullptr);
    bool         updateChannel(const QString &channelId, const QString &name, const SourceSpec &video,
                               const SourceSpec &audio, int bitrateKbps = 0, QString *error = nullptr);
    bool         removeChannel(const QString &channelId, QString *error = nullptr);
    bool         startChannel(const QString &channelId, QString *error = nullptr);
    bool         stopChannel(const QString &channelId, QString *error = nullptr);
    bool         setChannelBitrateKbps(const QString &channelId, int kbps, QString *error = nullptr);

public Q_SLOTS:
    void onUserOffline(qm::UserId user);

Q_SIGNALS:
    void channelsChanged();

private:
    struct Channel
    {
        QString    id;
        QString    name;
        SourceSpec video;
        SourceSpec audio;
        UserId     publisherId = 0;
        bool       wantRunning = false;
        int        adminBitrateKbps = 0; // 0 = use server default channel bitrate
        std::unique_ptr<ChannelPublisher> publisher;
        QSet<UserId> viewers;
        QHash<UserId, int> viewerBandwidthKbps;
    };

    void onList(const RequestContext &context, const proto::Envelope &envelope);
    void onJoin(const RequestContext &context, const proto::Envelope &envelope);
    void onLeave(const RequestContext &context, const proto::Envelope &envelope);
    void onBandwidth(const RequestContext &context, const proto::Envelope &envelope);

    void leaveViewer(const QString &channelId, UserId user);
    void broadcastEvent(proto::ChannelEvt::Change change, const Channel &channel);
    proto::ChannelInfo toInfo(const Channel &channel) const;
    bool startLocked(Channel &channel, QString *error);
    void stopLocked(Channel &channel);
    void applyChannelBitrateLocked(Channel &channel);
    int  targetBitrateKbpsLocked(const Channel &channel) const;
    void loadStore();
    void saveStore() const;
    static UserId makePublisherId(const QString &channelId);
    static QVariantMap sourceToMap(const SourceSpec &spec);
    static SourceSpec  sourceFromMap(const QVariantMap &map);
    static int clampBitrateKbps(int kbps);

    SessionRegistry            *m_sessions = nullptr;
    Sfu                        *m_sfu = nullptr;
    QString                     m_storePath;
    TransportMode               m_transportMode = TransportMode::Direct;
    int                         m_defaultBitrateKbps = 1200;
    mutable QMutex              m_mutex;
    QHash<QString, Channel *>   m_channels;
};

} // namespace qm
