#include "ChannelService.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>
#include <QVariantMap>

#include "common/EnvelopeFactory.h"
#include "common/Ids.h"
#include "common/Logger.h"
#include "net/SessionRegistry.h"
#include "streaming/ChannelPublisher.h"
#include "streaming/Sfu.h"

namespace qm {

namespace {

QString kindName(SourceKind kind)
{
    switch (kind) {
    case SourceKind::Camera:
        return QStringLiteral("camera");
    case SourceKind::Screen:
        return QStringLiteral("screen");
    case SourceKind::Microphone:
        return QStringLiteral("microphone");
    case SourceKind::Rtsp:
        return QStringLiteral("rtsp");
    case SourceKind::Url:
        return QStringLiteral("url");
    case SourceKind::File:
    default:
        return QStringLiteral("file");
    }
}

SourceKind kindFromName(const QString &name)
{
    if (name == QLatin1String("camera"))
        return SourceKind::Camera;
    if (name == QLatin1String("screen"))
        return SourceKind::Screen;
    if (name == QLatin1String("microphone"))
        return SourceKind::Microphone;
    if (name == QLatin1String("rtsp"))
        return SourceKind::Rtsp;
    if (name == QLatin1String("url"))
        return SourceKind::Url;
    return SourceKind::File;
}

} // namespace

ChannelService::ChannelService(SessionRegistry *sessions, Sfu *sfu, const QString &storePath,
                               QObject *parent)
    : QObject(parent)
    , m_sessions(sessions)
    , m_sfu(sfu)
    , m_storePath(storePath)
{
    loadStore();
    for (Channel *channel : m_channels) {
        if (channel->wantRunning)
            startLocked(*channel, nullptr);
    }
}

ChannelService::~ChannelService()
{
    QMutexLocker locker(&m_mutex);
    for (Channel *channel : m_channels) {
        stopLocked(*channel);
        delete channel;
    }
    m_channels.clear();
}

void ChannelService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    switch (envelope.body_case()) {
    case proto::Envelope::kChannelListReq:
        onList(context, envelope);
        break;
    case proto::Envelope::kChannelJoin:
        onJoin(context, envelope);
        break;
    case proto::Envelope::kChannelLeave:
        onLeave(context, envelope);
        break;
    case proto::Envelope::kChannelBandwidth:
        onBandwidth(context, envelope);
        break;
    default:
        break;
    }
}

void ChannelService::setTransportMode(TransportMode mode)
{
    QMutexLocker locker(&m_mutex);
    m_transportMode = mode;
    for (Channel *channel : m_channels)
        applyChannelBitrateLocked(*channel);
}

void ChannelService::setDefaultBitrateKbps(int kbps)
{
    QMutexLocker locker(&m_mutex);
    m_defaultBitrateKbps = clampBitrateKbps(kbps);
    for (Channel *channel : m_channels)
        applyChannelBitrateLocked(*channel);
}

QVariantList ChannelService::listForAdmin() const
{
    QMutexLocker locker(&m_mutex);
    QVariantList out;
    for (const Channel *channel : m_channels) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), channel->id);
        row.insert(QStringLiteral("name"), channel->name);
        row.insert(QStringLiteral("running"),
                   channel->publisher && channel->publisher->isRunning());
        row.insert(QStringLiteral("videoLabel"),
                   channel->video.location.isEmpty()
                       ? QStringLiteral("—")
                       : (channel->video.displayName.isEmpty() ? channel->video.location
                                                                : channel->video.displayName));
        row.insert(QStringLiteral("audioLabel"),
                   channel->audio.location.isEmpty()
                       ? QStringLiteral("—")
                       : (channel->audio.displayName.isEmpty() ? channel->audio.location
                                                                : channel->audio.displayName));
        row.insert(QStringLiteral("viewers"), channel->viewers.size());
        const int bitrate = channel->adminBitrateKbps > 0 ? channel->adminBitrateKbps
                                                          : m_defaultBitrateKbps;
        row.insert(QStringLiteral("bitrateKbps"), bitrate);
        row.insert(QStringLiteral("currentBandwidthKbps"), targetBitrateKbpsLocked(*channel));
        row.insert(QStringLiteral("videoLocation"), channel->video.location);
        row.insert(QStringLiteral("audioLocation"), channel->audio.location);
        out.append(row);
    }
    return out;
}

QString ChannelService::addChannel(const QString &name, const SourceSpec &video,
                                   const SourceSpec &audio, int bitrateKbps, QString *error)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        if (error)
            *error = QStringLiteral("Channel name is required");
        return {};
    }
    if (video.location.isEmpty() && audio.location.isEmpty()) {
        if (error)
            *error = QStringLiteral("Pick at least a video or audio source");
        return {};
    }

    auto *channel = new Channel;
    channel->id = Ids::uuid();
    channel->name = trimmed;
    channel->video = video;
    channel->audio = audio;
    channel->adminBitrateKbps = bitrateKbps > 0 ? clampBitrateKbps(bitrateKbps) : 0;
    channel->publisherId = makePublisherId(channel->id);
    channel->wantRunning = true;

    {
        QMutexLocker locker(&m_mutex);
        m_channels.insert(channel->id, channel);
        if (!startLocked(*channel, error)) {
            m_channels.remove(channel->id);
            delete channel;
            return {};
        }
        saveStore();
    }

    broadcastEvent(proto::ChannelEvt::ADDED, *channel);
    Q_EMIT channelsChanged();
    qCInfo(lcApp) << "Channel added" << channel->id << channel->name;
    return channel->id;
}

bool ChannelService::updateChannel(const QString &channelId, const QString &name,
                                   const SourceSpec &video, const SourceSpec &audio,
                                   int bitrateKbps, QString *error)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        if (error)
            *error = QStringLiteral("Channel name is required");
        return false;
    }
    if (video.location.isEmpty() && audio.location.isEmpty()) {
        if (error)
            *error = QStringLiteral("Pick at least a video or audio source");
        return false;
    }

    Channel *channel = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        channel = m_channels.value(channelId);
        if (!channel) {
            if (error)
                *error = QStringLiteral("Unknown channel");
            return false;
        }
        const bool wasRunning = channel->publisher && channel->publisher->isRunning();
        channel->name = trimmed;
        channel->video = video;
        channel->audio = audio;
        channel->adminBitrateKbps = bitrateKbps > 0 ? clampBitrateKbps(bitrateKbps) : 0;
        if (wasRunning) {
            stopLocked(*channel);
            if (!startLocked(*channel, error)) {
                channel->wantRunning = false;
                saveStore();
                return false;
            }
            channel->wantRunning = true;
        } else {
            applyChannelBitrateLocked(*channel);
        }
        saveStore();
    }

    broadcastEvent(proto::ChannelEvt::UPDATED, *channel);
    Q_EMIT channelsChanged();
    qCInfo(lcApp) << "Channel updated" << channelId << trimmed;
    return true;
}

bool ChannelService::removeChannel(const QString &channelId, QString *error)
{
    proto::ChannelInfo removedInfo;
    {
        QMutexLocker locker(&m_mutex);
        Channel *channel = m_channels.take(channelId);
        if (!channel) {
            if (error)
                *error = QStringLiteral("Unknown channel");
            return false;
        }
        const QSet<UserId> viewers = channel->viewers;
        stopLocked(*channel);
        removedInfo = toInfo(*channel);
        removedInfo.set_running(false);
        delete channel;
        saveStore();

        if (m_sfu) {
            for (UserId viewer : viewers)
                m_sfu->removeParticipant(channelId, viewer);
        }
    }

    if (m_sessions) {
        proto::Envelope env = EnvelopeFactory::event();
        auto *evt = env.mutable_channel_evt();
        evt->set_change(proto::ChannelEvt::REMOVED);
        *evt->mutable_channel() = removedInfo;
        m_sessions->broadcastAll(env);
    }
    Q_EMIT channelsChanged();
    return true;
}

bool ChannelService::startChannel(const QString &channelId, QString *error)
{
    QMutexLocker locker(&m_mutex);
    Channel *channel = m_channels.value(channelId);
    if (!channel) {
        if (error)
            *error = QStringLiteral("Unknown channel");
        return false;
    }
    channel->wantRunning = true;
    if (!startLocked(*channel, error))
        return false;
    saveStore();
    locker.unlock();
    broadcastEvent(proto::ChannelEvt::UPDATED, *channel);
    Q_EMIT channelsChanged();
    return true;
}

bool ChannelService::stopChannel(const QString &channelId, QString *error)
{
    QMutexLocker locker(&m_mutex);
    Channel *channel = m_channels.value(channelId);
    if (!channel) {
        if (error)
            *error = QStringLiteral("Unknown channel");
        return false;
    }
    channel->wantRunning = false;
    stopLocked(*channel);
    saveStore();
    locker.unlock();
    broadcastEvent(proto::ChannelEvt::UPDATED, *channel);
    Q_EMIT channelsChanged();
    return true;
}

bool ChannelService::setChannelBitrateKbps(const QString &channelId, int kbps, QString *error)
{
    QMutexLocker locker(&m_mutex);
    Channel *channel = m_channels.value(channelId);
    if (!channel) {
        if (error)
            *error = QStringLiteral("Unknown channel");
        return false;
    }
    channel->adminBitrateKbps = clampBitrateKbps(kbps);
    applyChannelBitrateLocked(*channel);
    saveStore();
    // Intentionally skip channelsChanged — admin UI updates the slider locally
    // without rebuilding the list mid-drag.
    return true;
}

void ChannelService::onUserOffline(UserId user)
{
    QStringList rooms;
    {
        QMutexLocker locker(&m_mutex);
        for (Channel *channel : m_channels) {
            if (channel->viewers.contains(user))
                rooms.append(channel->id);
        }
    }
    for (const QString &id : rooms)
        leaveViewer(id, user);
}

void ChannelService::onList(const RequestContext &context, const proto::Envelope &envelope)
{
    proto::Envelope reply = EnvelopeFactory::reply(envelope);
    auto *out = reply.mutable_channel_list_res();
    QMutexLocker locker(&m_mutex);
    for (const Channel *channel : m_channels)
        *out->add_channels() = toInfo(*channel);
    m_sessions->sendToConn(context.conn, reply);
}

void ChannelService::onJoin(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &req = envelope.channel_join();
    const QString channelId = QString::fromStdString(req.channel_id()).trimmed();
    if (channelId.isEmpty()) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INVALID,
                                                      QStringLiteral("Missing channel id")));
        return;
    }

    const int bw = clampBitrateKbps(int(req.bandwidth_kbps()));
    Channel *channel = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        channel = m_channels.value(channelId);
        if (!channel || !channel->publisher || !channel->publisher->isRunning()) {
            m_sessions->sendToConn(context.conn,
                                   EnvelopeFactory::error(envelope, proto::ERR_NOT_FOUND,
                                                          QStringLiteral("Channel offline")));
            return;
        }
        channel->viewers.insert(context.user);
        if (bw > 0)
            channel->viewerBandwidthKbps.insert(context.user, bw);
        applyChannelBitrateLocked(*channel);
    }

    if (m_sfu) {
        m_sfu->openRoom(channelId);
        m_sfu->addParticipant(channelId, context.user);
    }

    proto::Envelope reply = EnvelopeFactory::reply(envelope);
    auto *joined = reply.mutable_channel_joined();
    joined->set_channel_id(channelId.toStdString());
    joined->set_name(channel->name.toStdString());
    joined->set_session_id(channelId.toStdString());
    joined->set_publisher_id(channel->publisherId);
    if (m_sfu) {
        for (const proto::TrackInfo &track : m_sfu->tracksOf(channelId, channel->publisherId))
            *joined->add_tracks() = track;
    }
    m_sessions->sendToConn(context.conn, reply);
    qCInfo(lcApp) << "Channel join" << channelId << "user" << context.user << "bw" << bw;
}

void ChannelService::onLeave(const RequestContext &context, const proto::Envelope &envelope)
{
    const QString channelId = QString::fromStdString(envelope.channel_leave().channel_id());
    if (channelId.isEmpty())
        return;
    leaveViewer(channelId, context.user);
}

void ChannelService::onBandwidth(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &req = envelope.channel_bandwidth();
    const QString channelId = QString::fromStdString(req.channel_id()).trimmed();
    const int bw = clampBitrateKbps(int(req.bandwidth_kbps()));
    if (channelId.isEmpty() || bw <= 0)
        return;

    QMutexLocker locker(&m_mutex);
    Channel *channel = m_channels.value(channelId);
    if (!channel || !channel->viewers.contains(context.user))
        return;
    channel->viewerBandwidthKbps.insert(context.user, bw);
    applyChannelBitrateLocked(*channel);
}

void ChannelService::leaveViewer(const QString &channelId, UserId user)
{
    {
        QMutexLocker locker(&m_mutex);
        Channel *channel = m_channels.value(channelId);
        if (!channel || !channel->viewers.remove(user))
            return;
        channel->viewerBandwidthKbps.remove(user);
        applyChannelBitrateLocked(*channel);
    }
    if (m_sfu)
        m_sfu->removeParticipant(channelId, user);
}

void ChannelService::broadcastEvent(proto::ChannelEvt::Change change, const Channel &channel)
{
    if (!m_sessions)
        return;
    proto::Envelope env = EnvelopeFactory::event();
    auto *evt = env.mutable_channel_evt();
    evt->set_change(change);
    *evt->mutable_channel() = toInfo(channel);
    m_sessions->broadcastAll(env);
}

proto::ChannelInfo ChannelService::toInfo(const Channel &channel) const
{
    proto::ChannelInfo info;
    info.set_id(channel.id.toStdString());
    info.set_name(channel.name.toStdString());
    const bool running = channel.publisher && channel.publisher->isRunning();
    info.set_running(running);
    info.set_has_video(!channel.video.location.isEmpty());
    info.set_has_audio(!channel.audio.location.isEmpty());
    info.set_video_label((channel.video.displayName.isEmpty() ? channel.video.location
                                                              : channel.video.displayName)
                             .toStdString());
    info.set_audio_label((channel.audio.displayName.isEmpty() ? channel.audio.location
                                                              : channel.audio.displayName)
                             .toStdString());
    info.set_viewer_count(uint32_t(channel.viewers.size()));
    info.set_bitrate_kbps(uint32_t(targetBitrateKbpsLocked(channel)));
    return info;
}

bool ChannelService::startLocked(Channel &channel, QString *error)
{
    if (channel.publisher && channel.publisher->isRunning())
        return true;
    if (!m_sfu) {
        if (error)
            *error = QStringLiteral("SFU unavailable");
        return false;
    }

    channel.publisher = std::make_unique<ChannelPublisher>(m_sfu, channel.id, channel.publisherId,
                                                           channel.video, channel.audio, this);
    channel.publisher->setVideoBitrate(targetBitrateKbpsLocked(channel) * 1000);
    connect(channel.publisher.get(), &ChannelPublisher::errorOccurred, this,
            [this](const QString &message) {
                qCWarning(lcApp) << "ChannelPublisher:" << message;
            });
    if (!channel.publisher->start()) {
        if (error)
            *error = QStringLiteral("Cannot start capture for this channel");
        channel.publisher.reset();
        return false;
    }
    return true;
}

void ChannelService::applyChannelBitrateLocked(Channel &channel)
{
    if (!channel.publisher)
        return;
    channel.publisher->setVideoBitrate(targetBitrateKbpsLocked(channel) * 1000);
}

int ChannelService::targetBitrateKbpsLocked(const Channel &channel) const
{
    const int channelBase =
        channel.adminBitrateKbps > 0 ? channel.adminBitrateKbps : m_defaultBitrateKbps;
    int target = channelBase;
    if (!channel.viewerBandwidthKbps.isEmpty()) {
        target = 0;
        for (int kbps : channel.viewerBandwidthKbps)
            target = qMax(target, kbps);
        // Multicast shares one encode: keep at least this channel's admin floor.
        if (m_transportMode == TransportMode::Multicast)
            target = qMax(target, channelBase);
    }
    if (target <= 0)
        target = channelBase;
    return qBound(200, target, 8000);
}

int ChannelService::clampBitrateKbps(int kbps)
{
    if (kbps <= 0)
        return 0;
    return qBound(200, kbps, 8000);
}

void ChannelService::stopLocked(Channel &channel)
{
    if (channel.publisher) {
        channel.publisher->stop();
        channel.publisher.reset();
    }
}

void ChannelService::loadStore()
{
    QFile file(m_storePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray())
        return;
    for (const QJsonValue &value : doc.array()) {
        const QJsonObject obj = value.toObject();
        auto *channel = new Channel;
        channel->id = obj.value(QStringLiteral("id")).toString();
        channel->name = obj.value(QStringLiteral("name")).toString();
        channel->video = sourceFromMap(obj.value(QStringLiteral("video")).toObject().toVariantMap());
        channel->audio = sourceFromMap(obj.value(QStringLiteral("audio")).toObject().toVariantMap());
        channel->wantRunning = obj.value(QStringLiteral("wantRunning")).toBool(true);
        channel->adminBitrateKbps = obj.value(QStringLiteral("bitrateKbps")).toInt(0);
        if (channel->adminBitrateKbps > 0)
            channel->adminBitrateKbps = clampBitrateKbps(channel->adminBitrateKbps);
        if (channel->id.isEmpty() || channel->name.isEmpty()) {
            delete channel;
            continue;
        }
        channel->publisherId = makePublisherId(channel->id);
        m_channels.insert(channel->id, channel);
    }
}

void ChannelService::saveStore() const
{
    QJsonArray array;
    for (const Channel *channel : m_channels) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), channel->id);
        obj.insert(QStringLiteral("name"), channel->name);
        obj.insert(QStringLiteral("wantRunning"), channel->wantRunning);
        if (channel->adminBitrateKbps > 0)
            obj.insert(QStringLiteral("bitrateKbps"), channel->adminBitrateKbps);
        obj.insert(QStringLiteral("video"),
                   QJsonObject::fromVariantMap(sourceToMap(channel->video)));
        obj.insert(QStringLiteral("audio"),
                   QJsonObject::fromVariantMap(sourceToMap(channel->audio)));
        array.append(obj);
    }
    QFile file(m_storePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcApp) << "Cannot write channel store" << m_storePath;
        return;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

UserId ChannelService::makePublisherId(const QString &channelId)
{
    return 0xC000000000000000ULL | (quint64(qHash(channelId, 0xC0FFEEu)) & 0x0FFFFFFFFFFFFFFFULL);
}

QVariantMap ChannelService::sourceToMap(const SourceSpec &spec)
{
    QVariantMap map;
    map.insert(QStringLiteral("kind"), kindName(spec.kind));
    map.insert(QStringLiteral("location"), spec.location);
    map.insert(QStringLiteral("format"), spec.format);
    map.insert(QStringLiteral("displayName"), spec.displayName);
    QVariantMap options;
    for (auto it = spec.options.begin(); it != spec.options.end(); ++it)
        options.insert(it.key(), it.value());
    map.insert(QStringLiteral("options"), options);
    return map;
}

SourceSpec ChannelService::sourceFromMap(const QVariantMap &map)
{
    SourceSpec spec;
    spec.kind = kindFromName(map.value(QStringLiteral("kind")).toString());
    spec.location = map.value(QStringLiteral("location")).toString();
    spec.format = map.value(QStringLiteral("format")).toString();
    spec.displayName = map.value(QStringLiteral("displayName")).toString();
    const QVariantMap options = map.value(QStringLiteral("options")).toMap();
    for (auto it = options.begin(); it != options.end(); ++it)
        spec.options.insert(it.key(), it.value().toString());
    return spec;
}

} // namespace qm
