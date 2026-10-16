#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVideoSink>

#include "common/Ids.h"
#include "media/MediaTypes.h"
#include "messages.pb.h"

namespace qm {

class CallEngine;
class NetworkClient;
class ParticipantModel;
class SessionController;

/// Aim: call and meeting state machine for QML: invite / accept / hang up a 1:1 call,
/// create a meeting (get code) or join one by code, toggle mic / camera / speaker,
/// pick devices, share screen, bind video tiles.
class CallController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QAbstractItemModel *participants READ participants CONSTANT)
    Q_PROPERTY(QString meetingCode READ meetingCode NOTIFY meetingChanged)
    Q_PROPERTY(QString meetingTitle READ meetingTitle NOTIFY meetingChanged)
    Q_PROPERTY(QString channelName READ channelName NOTIFY channelChanged)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY channelsChanged)
    Q_PROPERTY(QString peerName READ peerName NOTIFY stateChanged)
    Q_PROPERTY(qint64 peerId READ peerId NOTIFY stateChanged)
    Q_PROPERTY(bool micEnabled READ micEnabled WRITE setMicEnabled NOTIFY micEnabledChanged)
    Q_PROPERTY(bool cameraEnabled READ cameraEnabled WRITE setCameraEnabled NOTIFY cameraEnabledChanged)
    Q_PROPERTY(bool speakerEnabled READ speakerEnabled WRITE setSpeakerEnabled NOTIFY speakerEnabledChanged)
    Q_PROPERTY(bool screenSharing READ screenSharing WRITE setScreenSharing NOTIFY screenSharingChanged)
    Q_PROPERTY(qreal micVolume READ micVolume WRITE setMicVolume NOTIFY micVolumeChanged)
    Q_PROPERTY(qreal speakerVolume READ speakerVolume WRITE setSpeakerVolume NOTIFY speakerVolumeChanged)
    Q_PROPERTY(QStringList videoDevices READ videoDevices NOTIFY devicesChanged)
    Q_PROPERTY(QStringList cameraDevices READ videoDevices NOTIFY devicesChanged)
    Q_PROPERTY(QStringList microphoneDevices READ microphoneDevices NOTIFY devicesChanged)
    Q_PROPERTY(QStringList speakerDevices READ speakerDevices NOTIFY devicesChanged)
    Q_PROPERTY(int videoIndex READ videoIndex WRITE setVideoIndex NOTIFY deviceSelectionChanged)
    Q_PROPERTY(int cameraIndex READ videoIndex WRITE setVideoIndex NOTIFY deviceSelectionChanged)
    Q_PROPERTY(int microphoneIndex READ microphoneIndex WRITE setMicrophoneIndex NOTIFY deviceSelectionChanged)
    Q_PROPERTY(int speakerIndex READ speakerIndex WRITE setSpeakerIndex NOTIFY deviceSelectionChanged)
    Q_PROPERTY(int bandwidthIndex READ bandwidthIndex WRITE setBandwidthIndex NOTIFY bandwidthChanged)
    Q_PROPERTY(QStringList bandwidthLabels READ bandwidthLabels CONSTANT)
    Q_PROPERTY(int bandwidthKbps READ bandwidthKbps NOTIFY bandwidthChanged)

public:
    enum class State { Idle, Outgoing, Incoming, InCall, InMeeting, Watching };
    Q_ENUM(State)

    CallController(NetworkClient *network, SessionController *session, CallEngine *engine,
                   ParticipantModel *participants, QObject *parent = nullptr);

    State               state() const;
    QAbstractItemModel *participants() const;
    QString             meetingCode() const;
    QString             meetingTitle() const;
    QString             channelName() const;
    QVariantList        channels() const;
    QString             peerName() const;
    qint64              peerId() const;
    bool                micEnabled() const;
    void                setMicEnabled(bool enabled);
    bool                cameraEnabled() const;
    void                setCameraEnabled(bool enabled);
    bool                speakerEnabled() const;
    void                setSpeakerEnabled(bool enabled);
    bool                screenSharing() const;
    void                setScreenSharing(bool enabled);
    qreal               micVolume() const;
    void                setMicVolume(qreal volume);
    qreal               speakerVolume() const;
    void                setSpeakerVolume(qreal volume);

    QStringList videoDevices() const;
    QStringList microphoneDevices() const;
    QStringList speakerDevices() const;
    int         videoIndex() const;
    void        setVideoIndex(int index);
    int         microphoneIndex() const;
    void        setMicrophoneIndex(int index);
    int         speakerIndex() const;
    void        setSpeakerIndex(int index);

    int         bandwidthIndex() const;
    void        setBandwidthIndex(int index);
    QStringList bandwidthLabels() const;
    int         bandwidthKbps() const;

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void callUser(qint64 userId, const QString &displayName, bool video);
    Q_INVOKABLE void acceptCall();
    Q_INVOKABLE void rejectCall();
    Q_INVOKABLE void hangup();

    Q_INVOKABLE void createMeeting(const QString &title, bool video);
    Q_INVOKABLE void joinMeeting(const QString &code, bool audio, bool video);
    Q_INVOKABLE void leaveMeeting();

    Q_INVOKABLE void refreshChannels();
    Q_INVOKABLE void joinChannel(const QString &channelId);
    Q_INVOKABLE void leaveChannel();
    Q_INVOKABLE void setChannelBandwidth(int bandwidthIndex);

    Q_INVOKABLE void attachVideoSink(qint64 userId, QVideoSink *sink);
    Q_INVOKABLE void detachVideoSink(qint64 userId);
    Q_INVOKABLE void attachLocalPreview(QVideoSink *sink);

Q_SIGNALS:
    void stateChanged();
    void meetingChanged();
    void channelChanged();
    void channelsChanged();
    void micEnabledChanged();
    void cameraEnabledChanged();
    void speakerEnabledChanged();
    void screenSharingChanged();
    void micVolumeChanged();
    void speakerVolumeChanged();
    void devicesChanged();
    void deviceSelectionChanged();
    void bandwidthChanged();
    void incomingCall(const QString &callerName, bool video);
    void callEnded(const QString &reason);
    void errorOccurred(const QString &message);

private:
    void onEvent(const proto::Envelope &envelope);
    void onCallInvite(const proto::CallInvite &invite);
    void onCallAnswer(const proto::CallAnswer &answer);
    void onCallHangup(const proto::CallHangup &hangup);
    void onMeetingJoined(const proto::MeetingJoined &joined);
    void onChannelJoined(const proto::ChannelJoined &joined);
    void onChannelEvent(const proto::ChannelEvt &event);
    void onParticipantEvent(const proto::ParticipantEvt &event);
    void setState(State state);
    void flushPendingSignals();
    void postMediaState();
    void enterMeeting(const proto::MeetingJoined &joined, bool video);
    void applySelectedDevices();
    void applyBandwidth();
    void postChannelBandwidth();
    void upsertChannelInfo(const proto::ChannelInfo &info);
    void removeChannelInfo(const QString &channelId);
    static int bandwidthKbpsForIndex(int index);

    NetworkClient     *m_network = nullptr;
    SessionController *m_session = nullptr;
    CallEngine        *m_engine = nullptr;
    ParticipantModel  *m_participants = nullptr;
    State              m_state = State::Idle;
    QString            m_sessionId;           // call_id, meeting_id, or channel_id
    QString            m_meetingCode;
    QString            m_meetingTitle;
    QString            m_channelName;
    QVariantList       m_channels;
    UserId             m_channelPublisher = 0;
    QString            m_peerName;
    UserId             m_peer = 0;
    bool               m_video = true;
    bool               m_micEnabled = true;
    bool               m_cameraEnabled = true;
    bool               m_speakerEnabled = true;
    bool               m_screenSharing = false;
    qreal              m_micVolume = 1.0;
    qreal              m_speakerVolume = 1.0;
    QList<SourceSpec>  m_videoSources;   // cameras + optional screen capture
    QList<SourceSpec>  m_microphones;
    QStringList        m_speakerIds;
    QStringList        m_speakerNames;
    int                m_videoIndex = 0;
    int                m_microphoneIndex = 0;
    int                m_speakerIndex = 0;
    int                m_bandwidthIndex = 1; // Medium (1200 kbps)
    QList<proto::RtcSignal> m_pendingSignals;
};

} // namespace qm
