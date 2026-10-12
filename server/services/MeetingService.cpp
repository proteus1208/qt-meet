#include "MeetingService.h"

namespace qm {

MeetingService::MeetingService(StorageManager *storage, SessionRegistry *sessions, Sfu *sfu, QObject *parent)
    : QObject(parent)
    , m_storage(storage)
    , m_sessions(sessions)
    , m_sfu(sfu)
{
}

void MeetingService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void MeetingService::onUserOffline(UserId user)
{
    Q_UNUSED(user)
}

void MeetingService::onSfuSignal(UserId user, const proto::RtcSignal &signal)
{
    Q_UNUSED(user)
    Q_UNUSED(signal)
}

void MeetingService::onCallInvite(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void MeetingService::onCallAnswer(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void MeetingService::onCallHangup(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void MeetingService::onRtcSignal(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void MeetingService::onMediaState(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void MeetingService::onMeetingCreate(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void MeetingService::onMeetingJoin(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void MeetingService::onMeetingLeave(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void MeetingService::leaveRoom(const QString &roomId, UserId user)
{
    Q_UNUSED(roomId)
    Q_UNUSED(user)
}

void MeetingService::notifyParticipants(const Room &room, const proto::ParticipantEvt &event)
{
    Q_UNUSED(room)
    Q_UNUSED(event)
}

} // namespace qm
