#include "MeetingService.h"

#include "common/EnvelopeFactory.h"
#include "common/Ids.h"
#include "common/Logger.h"
#include "net/SessionRegistry.h"
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
    QStringList toClose;
    for (auto it = m_rooms.begin(); it != m_rooms.end(); ++it) {
        if (it->members.contains(user))
            toClose.append(it.key());
    }
    locker.unlock();
    for (const QString &roomId : toClose)
        leaveRoom(roomId, user);
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
        m_rooms.insert(callId, room);
    }

    proto::Envelope forward = EnvelopeFactory::event();
    auto *out = forward.mutable_call_invite();
    out->set_call_id(callId.toStdString());
    out->set_from_user_id(context.user);
    out->set_to_user_id(to);
    out->set_kind(inv.kind());
    m_sessions->sendTo(to, forward);

    // Echo call_id to caller so both sides share it.
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

    // Notify the other party.
    for (UserId member : room.members) {
        if (member == context.user)
            continue;
        proto::Envelope forward = EnvelopeFactory::event();
        *forward.mutable_call_answer() = ans;
        m_sessions->sendTo(member, forward);
    }

    if (!ans.accepted()) {
        leaveRoom(callId, context.user);
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
    leaveRoom(callId, context.user);
}

void MeetingService::onRtcSignal(const RequestContext &context, const proto::Envelope &envelope)
{
    if (m_sfu)
        m_sfu->applySignal(context.user, envelope.rtc_signal());
}

void MeetingService::onMediaState(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
    // Presence busy / in-call updates can fan out here in a later polish pass.
}

void MeetingService::onMeetingCreate(const RequestContext &context, const proto::Envelope &envelope)
{
    m_sessions->sendToConn(context.conn,
                           EnvelopeFactory::error(envelope, proto::ERR_BUSY,
                                                  QStringLiteral("Meetings arrive in Phase 7")));
}

void MeetingService::onMeetingJoin(const RequestContext &context, const proto::Envelope &envelope)
{
    m_sessions->sendToConn(context.conn,
                           EnvelopeFactory::error(envelope, proto::ERR_BUSY,
                                                  QStringLiteral("Meetings arrive in Phase 7")));
}

void MeetingService::onMeetingLeave(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void MeetingService::leaveRoom(const QString &roomId, UserId user)
{
    if (m_sfu)
        m_sfu->removeParticipant(roomId, user);

    QMutexLocker locker(&m_mutex);
    auto it = m_rooms.find(roomId);
    if (it == m_rooms.end())
        return;
    it->members.remove(user);
    if (it->members.isEmpty() || it->isCall) {
        if (!it->code.isEmpty())
            m_codeToRoom.remove(it->code);
        if (m_sfu)
            m_sfu->closeRoom(roomId);
        m_rooms.erase(it);
    }
}

void MeetingService::notifyParticipants(const Room &room, const proto::ParticipantEvt &event)
{
    proto::Envelope env = EnvelopeFactory::event();
    *env.mutable_participant_evt() = event;
    for (UserId member : room.members)
        m_sessions->sendTo(member, env);
}

} // namespace qm
