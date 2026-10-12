#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>

#include <memory>

#include "RtcConfig.h"
#include "messages.pb.h"

namespace rtc {
class PeerConnection;
class Track;
}

namespace qm {

/// Aim: one libdatachannel PeerConnection with its audio/video tracks. Used on the client
/// (one peer to the SFU) and on the server (one peer per participant inside the SFU).
/// Signaling is exchanged as proto::RtcSignal through the TLS control channel.
class RtcPeer : public QObject
{
    Q_OBJECT

public:
    enum class Role      { Client, Sfu };
    enum class State     { New, Connecting, Connected, Disconnected, Failed, Closed };
    enum class TrackKind { Audio, Video };
    enum class Direction { SendOnly, RecvOnly, SendRecv };
    Q_ENUM(State)

    RtcPeer(const RtcConfig &config, Role role, const QString &sessionId, QObject *parent = nullptr);
    ~RtcPeer() override;

    void createOffer();
    void applyRemoteSignal(const proto::RtcSignal &signal);

    int  addTrack(TrackKind kind, const QString &mid, quint32 ssrc, Direction direction);
    void removeTrack(int trackId);

    bool sendFrame(int trackId, const QByteArray &frame, quint32 rtpTimestamp);   // packetized by the track handler chain
    bool sendRtp(int trackId, const QByteArray &rtpPacket);                       // SFU forwarding, already packetized
    void requestKeyFrame(int trackId);                                            // sends PLI
    void setBitrateHint(int trackId, int bitsPerSecond);

    void    close();
    State   state() const;
    QString sessionId() const;

Q_SIGNALS:
    void localSignal(const qm::proto::RtcSignal &signal);
    void stateChanged(qm::RtcPeer::State state);
    void trackOpened(int trackId);
    void trackClosed(int trackId);
    void rtpReceived(int trackId, const QByteArray &rtpPacket);
    void keyFrameRequested(int trackId);
    void bitrateEstimate(int bitsPerSecond);

private:
    void setupCallbacks();
    void attachSendChain(int trackId, TrackKind kind, quint32 ssrc);

    RtcConfig                                m_config;
    Role                                     m_role;
    QString                                  m_sessionId;
    std::shared_ptr<rtc::PeerConnection>     m_pc;
    QHash<int, std::shared_ptr<rtc::Track>>  m_tracks;
    int                                      m_nextTrackId = 1;
    State                                    m_state = State::New;
};

} // namespace qm
