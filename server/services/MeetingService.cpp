#include "MeetingService.h"

#include <QDateTime>

#include "common/EnvelopeFactory.h"
#include "common/Ids.h"
#include "common/Logger.h"
#include "net/SessionRegistry.h"
#include "storage/StorageManager.h"
#include "streaming/Sfu.h"

namespace qm {

MeetingService::MeetingService(StorageManager *storage, SessionRegistry *sessions, Sfu *sfu,
                               QObject *parent)
    : QObject(parent)
    , m_storage(storage)
    , m_sessions(sessions)
    , m_sfu(sfu)
{
    if (m_sfu) {
        connect(m_sfu, &Sfu::signalForUser, this, &MeetingService::onSfuSignal);
    }
}

void MeetingService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    switch (envelope.body_case()) {
    case proto::Envelope::kCallInvite:
        onCallInvite(context, envelope);
        break;
    case proto::Envelope::kCallAnswer:
        onCallAnswer(context, envelope);
        break;
    case proto::Envelope::kCallHangup:
        onCallHangup(context, envelope);
        break;
    case proto::Envelope::kRtcSignal:
        onRtcSignal(context, envelope);
        break;
    case proto::Envelope::kMediaState:
        onMediaState(context, envelope);
        break;
    case proto::Envelope::kMeetingCreate:
        onMeetingCreate(context, envelope);
        break;
    case proto::Envelope::kMeetingJoin:
        onMeetingJoin(context, envelope);
        break;
    case proto::Envelope::kMeetingLeave:
        onMeetingLeave(context, envelope);
        break;
    default:
        break;
    }
}

void MeetingService::onUserOffline(UserId user)
{
    if (m_sfu)
        m_sfu->removeEverywhere(user);

    QMutexLocker locker(&m_mutex);
    QStringList toLeave;
    for (auto it = m_rooms.begin(); it != m_rooms.end(); ++it) {
        if (it->members.contains(user))
            toLeave.append(it.key());
    }
    locker.unlock();
    for (const QString &roomId : toLeave)
        leaveRoom(roomId, user, true);
}

void MeetingService::onSfuSignal(UserId user, const proto::RtcSignal &signal)
{
    proto::Envelope env = EnvelopeFactory::event();
    *env.mutable_rtc_signal() = signal;
    m_sessions->sendTo(user, env);
}

void MeetingService::onCallInvite(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &inv = envelope.call_invite();
    const UserId to = inv.to_user_id();
    if (to == 0 || to == context.user) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INVALID,
                                                      QStringLiteral("Invalid callee")));
        return;
    }
    if (!m_sessions->isOnline(to)) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_NOT_FOUND,
                                                      QStringLiteral("User offline")));
        return;
    }

    QString callId = QString::fromStdString(inv.call_id());
    if (callId.isEmpty())
        callId = Ids::uuid();

    {
        QMutexLocker locker(&m_mutex);
        Room room;
        room.roomId = callId;
        room.host = context.user;
        room.members = {context.user, to};
        room.isCall = true;
        const bool video = inv.kind() == proto::CALL_VIDEO;
        room.media.insert(context.user, {true, video});
        room.media.insert(to, {true, video});
        m_rooms.insert(callId, room);
    }

    proto::Envelope forward = EnvelopeFactory::event();
    auto *out = forward.mutable_call_invite();
    out->set_call_id(callId.toStdString());
    out->set_from_user_id(context.user);
    out->set_to_user_id(to);
    out->set_kind(inv.kind());
    m_sessions->sendTo(to, forward);

    proto::Envelope ack = EnvelopeFactory::reply(envelope);
    *ack.mutable_call_invite() = *out;
    m_sessions->sendToConn(context.conn, ack);

    qCInfo(lcApp) << "Call invite" << callId << "from" << context.user << "to" << to;
}

void MeetingService::onCallAnswer(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &ans = envelope.call_answer();
    const QString callId = QString::fromStdString(ans.call_id());

    Room room;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_rooms.contains(callId)) {
            m_sessions->sendToConn(context.conn,
                                   EnvelopeFactory::error(envelope, proto::ERR_NOT_FOUND,
                                                          QStringLiteral("Unknown call")));
            return;
        }
        room = m_rooms.value(callId);
        if (!room.members.contains(context.user)) {
            m_sessions->sendToConn(context.conn,
                                   EnvelopeFactory::error(envelope, proto::ERR_FORBIDDEN,
                                                          QStringLiteral("Not a participant")));
            return;
        }
    }

    for (UserId member : room.members) {
        if (member == context.user)
            continue;
        proto::Envelope forward = EnvelopeFactory::event();
        *forward.mutable_call_answer() = ans;
        m_sessions->sendTo(member, forward);
    }

    if (!ans.accepted()) {
        leaveRoom(callId, context.user, false);
        return;
    }

    if (m_sfu) {
        m_sfu->openRoom(callId);
        for (UserId member : room.members)
            m_sfu->addParticipant(callId, member);
    }
    qCInfo(lcApp) << "Call accepted" << callId;
}

void MeetingService::onCallHangup(const RequestContext &context, const proto::Envelope &envelope)
{
    const QString callId = QString::fromStdString(envelope.call_hangup().call_id());
    Room room;
    {
        QMutexLocker locker(&m_mutex);
        room = m_rooms.value(callId);
    }
    for (UserId member : room.members) {
        if (member == context.user)
            continue;
        proto::Envelope forward = EnvelopeFactory::event();
        *forward.mutable_call_hangup() = envelope.call_hangup();
        m_sessions->sendTo(member, forward);
    }
    leaveRoom(callId, context.user, false);
}

void MeetingService::onRtcSignal(const RequestContext &context, const proto::Envelope &envelope)
{
    if (m_sfu)
        m_sfu->applySignal(context.user, envelope.rtc_signal());
}

void MeetingService::onMediaState(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &state = envelope.media_state();
    const QString roomId = QString::fromStdString(state.session_id());

    Room room;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_rooms.contains(roomId) || !m_rooms[roomId].members.contains(context.user))
            return;
        auto &media = m_rooms[roomId].media[context.user];
        media.audioOn = state.audio_on();
        media.videoOn = state.video_on();
        room = m_rooms.value(roomId);
    }

    proto::ParticipantEvt evt;
    evt.set_meeting_id(roomId.toStdString());
    evt.set_change(proto::ParticipantEvt::UPDATED);
    *evt.mutable_participant() = makeParticipant(room, context.user);
    notifyParticipants(room, evt);
}

void MeetingService::onMeetingCreate(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &req = envelope.meeting_create();
    const QString title = QString::fromStdString(req.title()).trimmed();
    const bool video = req.video();

    QString code;
    for (int attempt = 0; attempt < 8; ++attempt) {
        code = Ids::meetingCode(8);
        if (!m_storage->meetingByCode(code).result())
            break;
        code.clear();
    }
    if (code.isEmpty()) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INTERNAL,
                                                      QStringLiteral("Cannot allocate meeting code")));
        return;
    }

    const ConversationId conversation =
        m_storage->createMeetingConversation(QList<UserId>{context.user}).result();
    if (conversation == 0) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INTERNAL,
                                                      QStringLiteral("Cannot create meeting chat")));
        return;
    }

    const QString meetingId = Ids::uuid();
    MeetingRecord record;
    record.meetingId = meetingId;
    record.code = code;
    record.title = title.isEmpty() ? QStringLiteral("Meeting") : title;
    record.host = context.user;
    record.conversationId = conversation;
    record.active = true;
    record.createdMs = QDateTime::currentMSecsSinceEpoch();
    if (!m_storage->saveMeeting(record).result()) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INTERNAL,
                                                      QStringLiteral("Cannot save meeting")));
        return;
    }

    Room room;
    room.roomId = meetingId;
    room.code = code;
    room.title = record.title;
    room.host = context.user;
    room.conversationId = conversation;
    room.members = {context.user};
    room.media.insert(context.user, {true, video});
    room.isCall = false;

    {
        QMutexLocker locker(&m_mutex);
        m_rooms.insert(meetingId, room);
        m_codeToRoom.insert(code.toUpper(), meetingId);
    }

    if (m_sfu) {
        m_sfu->openRoom(meetingId);
        m_sfu->addParticipant(meetingId, context.user);
    }

    proto::Envelope created = EnvelopeFactory::reply(envelope);
    auto *out = created.mutable_meeting_created();
    out->set_meeting_id(meetingId.toStdString());
    out->set_code(code.toStdString());
    m_sessions->sendToConn(context.conn, created);

    // Full join snapshot as an event so the client can start media without correlating replies.
    proto::Envelope joined = EnvelopeFactory::event();
    *joined.mutable_meeting_joined() = makeJoined(room);
    m_sessions->sendTo(context.user, joined);

    qCInfo(lcApp) << "Meeting created" << meetingId << "code" << code << "host" << context.user;
}

void MeetingService::onMeetingJoin(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &req = envelope.meeting_join();
    const QString code = QString::fromStdString(req.code()).trimmed().toUpper();
    if (code.isEmpty()) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INVALID,
                                                      QStringLiteral("Missing meeting code")));
        return;
    }

    Room room;
    QString error;
    if (!ensureRoomLoaded(code, &room, &error)) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_NOT_FOUND, error));
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        auto it = m_rooms.find(room.roomId);
        if (it == m_rooms.end()) {
            m_sessions->sendToConn(context.conn,
                                   EnvelopeFactory::error(envelope, proto::ERR_NOT_FOUND,
                                                          QStringLiteral("Meeting ended")));
            return;
        }
        if (it->members.contains(context.user)) {
            room = *it;
        } else {
            if (it->members.size() >= kMaxMeetingMembers) {
                m_sessions->sendToConn(context.conn,
                                       EnvelopeFactory::error(envelope, proto::ERR_BUSY,
                                                              QStringLiteral("Meeting is full")));
                return;
            }
            it->members.insert(context.user);
            it->media.insert(context.user, {req.audio(), req.video()});
            room = *it;
        }
    }

    if (room.conversationId != 0)
        m_storage->addMember(room.conversationId, context.user).result();

    if (m_sfu) {
        m_sfu->openRoom(room.roomId);
        m_sfu->addParticipant(room.roomId, context.user);
    }

    proto::Envelope reply = EnvelopeFactory::reply(envelope);
    *reply.mutable_meeting_joined() = makeJoined(room);
    m_sessions->sendToConn(context.conn, reply);

    proto::ParticipantEvt evt;
    evt.set_meeting_id(room.roomId.toStdString());
    evt.set_change(proto::ParticipantEvt::JOINED);
    *evt.mutable_participant() = makeParticipant(room, context.user);
    notifyParticipants(room, evt, context.user);

    qCInfo(lcApp) << "Meeting join" << room.roomId << "user" << context.user;
}

void MeetingService::onMeetingLeave(const RequestContext &context, const proto::Envelope &envelope)
{
    const QString meetingId = QString::fromStdString(envelope.meeting_leave().meeting_id());
    if (meetingId.isEmpty())
        return;
    leaveRoom(meetingId, context.user, true);
}

void MeetingService::leaveRoom(const QString &roomId, UserId user, bool notifyLeft)
{
    Room before;
    Room remaining;
    bool roomGone = false;
    bool wasMeeting = false;
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_rooms.find(roomId);
        if (it == m_rooms.end() || !it->members.contains(user))
            return;

        before = *it;
        wasMeeting = !it->isCall;
        it->members.remove(user);
        it->media.remove(user);

        if (it->members.isEmpty() || it->isCall) {
            if (!it->code.isEmpty())
                m_codeToRoom.remove(it->code.toUpper());
            m_rooms.erase(it);
            roomGone = true;
        } else {
            remaining = *it;
        }
    }

    if (m_sfu) {
        if (roomGone)
            m_sfu->closeRoom(roomId);
        else
            m_sfu->removeParticipant(roomId, user);
    }

    if (roomGone && wasMeeting && m_storage)
        m_storage->closeMeeting(roomId).result();

    if (notifyLeft && wasMeeting) {
        proto::ParticipantEvt evt;
        evt.set_meeting_id(roomId.toStdString());
        evt.set_change(proto::ParticipantEvt::LEFT);
        auto *p = evt.mutable_participant();
        p->mutable_user()->set_id(user);
        p->mutable_user()->set_display_name(m_sessions->displayName(user).toStdString());

        const QSet<UserId> recipients = roomGone ? before.members : remaining.members;
        for (UserId member : recipients) {
            if (member == user)
                continue;
            proto::Envelope env = EnvelopeFactory::event();
            *env.mutable_participant_evt() = evt;
            m_sessions->sendTo(member, env);
        }
    }
}

void MeetingService::notifyParticipants(const Room &room, const proto::ParticipantEvt &event,
                                        UserId except)
{
    proto::Envelope env = EnvelopeFactory::event();
    *env.mutable_participant_evt() = event;
    for (UserId member : room.members) {
        if (member == except)
            continue;
        m_sessions->sendTo(member, env);
    }
}

proto::Participant MeetingService::makeParticipant(const Room &room, UserId user) const
{
    proto::Participant p;
    p.mutable_user()->set_id(user);
    p.mutable_user()->set_display_name(m_sessions->displayName(user).toStdString());
    const MemberMedia media = room.media.value(user, {true, true});
    p.set_audio_on(media.audioOn);
    p.set_video_on(media.videoOn);
    if (m_sfu) {
        const QList<proto::TrackInfo> tracks = m_sfu->tracksOf(room.roomId, user);
        for (const proto::TrackInfo &t : tracks)
            *p.add_tracks() = t;
    }
    return p;
}

proto::MeetingJoined MeetingService::makeJoined(const Room &room) const
{
    proto::MeetingJoined joined;
    joined.set_meeting_id(room.roomId.toStdString());
    joined.set_title(room.title.toStdString());
    joined.set_code(room.code.toStdString());
    joined.set_conversation_id(room.conversationId);
    for (UserId member : room.members)
        *joined.add_participants() = makeParticipant(room, member);
    return joined;
}

bool MeetingService::ensureRoomLoaded(const QString &code, Room *outRoom, QString *error)
{
    const QString key = code.trimmed().toUpper();
    {
        QMutexLocker locker(&m_mutex);
        if (const QString roomId = m_codeToRoom.value(key); !roomId.isEmpty()) {
            if (m_rooms.contains(roomId)) {
                *outRoom = m_rooms.value(roomId);
                return true;
            }
        }
    }

    const auto record = m_storage->meetingByCode(key).result();
    if (!record || !record->active) {
        if (error)
            *error = QStringLiteral("Unknown meeting code");
        return false;
    }

    Room room;
    room.roomId = record->meetingId;
    room.code = record->code;
    room.title = record->title;
    room.host = record->host;
    room.conversationId = record->conversationId;
    room.isCall = false;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_rooms.contains(room.roomId)) {
            m_rooms.insert(room.roomId, room);
            m_codeToRoom.insert(room.code.toUpper(), room.roomId);
        }
        *outRoom = m_rooms.value(room.roomId);
    }
    return true;
}

} // namespace qm
