#include "RtcPeer.h"

#include <QRandomGenerator>

#include "RtpPacketizer.h"
#include "common/Logger.h"

#if __has_include(<rtc/rtc.hpp>)
#include <rtc/rtc.hpp>
#define QM_HAVE_DATACHANNEL 1
#endif

namespace qm {

RtcPeer::RtcPeer(const RtcConfig &config, Role role, const QString &sessionId, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_role(role)
    , m_sessionId(sessionId)
{
    setupCallbacks();
}

RtcPeer::~RtcPeer()
{
    close();
}

void RtcPeer::setupCallbacks()
{
#ifdef QM_HAVE_DATACHANNEL
    rtc::Configuration conf;
    conf.portRangeBegin = m_config.portRangeBegin;
    conf.portRangeEnd = m_config.portRangeEnd;
    if (!m_config.bindAddress.isEmpty())
        conf.bindAddress = m_config.bindAddress.toStdString();
    if (!m_config.turnUrl.isEmpty())
        conf.iceServers.emplace_back(m_config.turnUrl.toStdString());

    m_pc = std::make_shared<rtc::PeerConnection>(conf);

    m_pc->onLocalDescription([this](rtc::Description description) {
        proto::RtcSignal signal;
        signal.set_session_id(m_sessionId.toStdString());
        auto *sdp = signal.mutable_sdp();
        sdp->set_type(description.typeString());
        sdp->set_sdp(std::string(description));
        Q_EMIT localSignal(signal);
    });

    m_pc->onLocalCandidate([this](rtc::Candidate candidate) {
        proto::RtcSignal signal;
        signal.set_session_id(m_sessionId.toStdString());
        auto *c = signal.mutable_candidate();
        c->set_candidate(std::string(candidate));
        // Newer libdatachannel: mid() returns std::string (not optional).
        c->set_mid(candidate.mid());
        Q_EMIT localSignal(signal);
    });

    m_pc->onStateChange([this](rtc::PeerConnection::State state) {
        State mapped = State::Connecting;
        switch (state) {
        case rtc::PeerConnection::State::New:
            mapped = State::New;
            break;
        case rtc::PeerConnection::State::Connecting:
            mapped = State::Connecting;
            break;
        case rtc::PeerConnection::State::Connected:
            mapped = State::Connected;
            break;
        case rtc::PeerConnection::State::Disconnected:
            mapped = State::Disconnected;
            break;
        case rtc::PeerConnection::State::Failed:
            mapped = State::Failed;
            break;
        case rtc::PeerConnection::State::Closed:
            mapped = State::Closed;
            break;
        }
        m_state = mapped;
        Q_EMIT stateChanged(mapped);
    });

    m_pc->onTrack([this](std::shared_ptr<rtc::Track> track) {
        const int id = m_nextTrackId++;
        m_tracks.insert(id, track);
        // Newer API: onMessage(binaryCallback, stringCallback) — two args required.
        track->onMessage(
            [this, id](rtc::binary data) {
                const QByteArray packet(reinterpret_cast<const char *>(data.data()),
                                        int(data.size()));
                Q_EMIT rtpReceived(id, packet);
            },
            nullptr);
        track->onOpen([this, id] { Q_EMIT trackOpened(id); });
        track->onClosed([this, id] {
            Q_EMIT trackClosed(id);
            m_tracks.remove(id);
        });
    });
#else
    qCWarning(lcRtc) << "RtcPeer built without libdatachannel (set DATACHANNEL_DIR)";
#endif
}

void RtcPeer::createOffer()
{
#ifdef QM_HAVE_DATACHANNEL
    if (!m_pc)
        return;
    try {
        m_pc->setLocalDescription(rtc::Description::Type::Offer);
    } catch (const std::exception &ex) {
        qCWarning(lcRtc) << "createOffer failed:" << ex.what();
    }
#endif
}

void RtcPeer::applyRemoteSignal(const proto::RtcSignal &signal)
{
#ifdef QM_HAVE_DATACHANNEL
    if (!m_pc)
        return;
    try {
        if (signal.has_sdp()) {
            const auto &sdp = signal.sdp();
            rtc::Description desc(sdp.sdp(), sdp.type());
            m_pc->setRemoteDescription(desc);
            if (sdp.type() == "offer")
                m_pc->setLocalDescription(rtc::Description::Type::Answer);
        } else if (signal.has_candidate()) {
            const auto &c = signal.candidate();
            rtc::Candidate candidate(c.candidate(), c.mid());
            m_pc->addRemoteCandidate(candidate);
        }
    } catch (const std::exception &ex) {
        qCWarning(lcRtc) << "applyRemoteSignal failed:" << ex.what();
    }
#else
    Q_UNUSED(signal)
#endif
}

int RtcPeer::addTrack(TrackKind kind, const QString &mid, quint32 ssrc, Direction direction)
{
#ifdef QM_HAVE_DATACHANNEL
    if (!m_pc)
        return -1;

    const quint32 useSsrc = ssrc ? ssrc : quint32(QRandomGenerator::global()->generate());
    const QString useMid = mid.isEmpty()
                               ? (kind == TrackKind::Video ? QStringLiteral("video")
                                                           : QStringLiteral("audio"))
                               : mid;
    const RtpCodec codec = kind == TrackKind::Video ? RtpCodec::H264 : RtpCodec::Opus;

    rtc::Description::Direction dir = rtc::Description::Direction::SendRecv;
    if (direction == Direction::SendOnly)
        dir = rtc::Description::Direction::SendOnly;
    else if (direction == Direction::RecvOnly)
        dir = rtc::Description::Direction::RecvOnly;

    std::shared_ptr<rtc::Track> track;
    if (kind == TrackKind::Video) {
        rtc::Description::Video media(useMid.toStdString(), dir);
        media.addH264Codec(RtpPacketizer::payloadType(codec));
        media.addSSRC(useSsrc, useMid.toStdString());
        track = m_pc->addTrack(media);
    } else {
        rtc::Description::Audio media(useMid.toStdString(), dir);
        media.addOpusCodec(RtpPacketizer::payloadType(codec));
        media.addSSRC(useSsrc, useMid.toStdString());
        track = m_pc->addTrack(media);
    }

    const int id = m_nextTrackId++;
    m_tracks.insert(id, track);

    if (direction != Direction::RecvOnly) {
        if (auto handler = RtpPacketizer::createChain(codec, useSsrc, useMid, m_config.mtu))
            track->setMediaHandler(handler);
    }

    track->onMessage(
        [this, id](rtc::binary data) {
            const QByteArray packet(reinterpret_cast<const char *>(data.data()), int(data.size()));
            Q_EMIT rtpReceived(id, packet);
        },
        nullptr);
    track->onOpen([this, id] { Q_EMIT trackOpened(id); });
    track->onClosed([this, id] {
        Q_EMIT trackClosed(id);
        m_tracks.remove(id);
    });

    Q_UNUSED(m_role)
    return id;
#else
    Q_UNUSED(kind)
    Q_UNUSED(mid)
    Q_UNUSED(ssrc)
    Q_UNUSED(direction)
    return -1;
#endif
}

void RtcPeer::removeTrack(int trackId)
{
#ifdef QM_HAVE_DATACHANNEL
    if (auto track = m_tracks.take(trackId))
        track->close();
#else
    Q_UNUSED(trackId)
#endif
}

bool RtcPeer::sendFrame(int trackId, const QByteArray &frame, quint32 rtpTimestamp)
{
#ifdef QM_HAVE_DATACHANNEL
    auto track = m_tracks.value(trackId);
    if (!track || !track->isOpen() || frame.isEmpty())
        return false;
    Q_UNUSED(rtpTimestamp)
    try {
        track->send(reinterpret_cast<const std::byte *>(frame.constData()), size_t(frame.size()));
        return true;
    } catch (const std::exception &ex) {
        qCWarning(lcRtc) << "sendFrame failed:" << ex.what();
        return false;
    }
#else
    Q_UNUSED(trackId)
    Q_UNUSED(frame)
    Q_UNUSED(rtpTimestamp)
    return false;
#endif
}

bool RtcPeer::sendRtp(int trackId, const QByteArray &rtpPacket)
{
    return sendFrame(trackId, rtpPacket, 0);
}

void RtcPeer::requestKeyFrame(int trackId)
{
#ifdef QM_HAVE_DATACHANNEL
    auto track = m_tracks.value(trackId);
    if (!track)
        return;
    try {
        track->requestKeyframe();
    } catch (...) {
        Q_EMIT keyFrameRequested(trackId);
    }
#else
    Q_EMIT keyFrameRequested(trackId);
#endif
}

void RtcPeer::setBitrateHint(int trackId, int bitsPerSecond)
{
    Q_UNUSED(trackId)
    Q_UNUSED(bitsPerSecond)
}

void RtcPeer::close()
{
#ifdef QM_HAVE_DATACHANNEL
    for (auto it = m_tracks.begin(); it != m_tracks.end(); ++it) {
        if (it.value())
            it.value()->close();
    }
    m_tracks.clear();
    if (m_pc) {
        m_pc->close();
        m_pc.reset();
    }
#endif
    m_state = State::Closed;
}

RtcPeer::State RtcPeer::state() const
{
    return m_state;
}

QString RtcPeer::sessionId() const
{
    return m_sessionId;
}

void RtcPeer::attachSendChain(int trackId, TrackKind kind, quint32 ssrc)
{
    Q_UNUSED(trackId)
    Q_UNUSED(kind)
    Q_UNUSED(ssrc)
}

} // namespace qm
