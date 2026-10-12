#include "ParticipantModel.h"

namespace qm {

ParticipantModel::ParticipantModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ParticipantModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant ParticipantModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return {};
}

QHash<int, QByteArray> ParticipantModel::roleNames() const
{
    return {
        {UserIdRole, "userId"},
        {DisplayNameRole, "displayName"},
        {AudioOnRole, "audioOn"},
        {VideoOnRole, "videoOn"},
        {SpeakingRole, "speaking"},
        {IsLocalRole, "isLocal"},
    };
}

void ParticipantModel::setParticipants(const QList<proto::Participant> &participants, UserId localUser)
{
    Q_UNUSED(participants)
    m_localUser = localUser;
}

void ParticipantModel::upsert(const proto::Participant &participant)
{
    Q_UNUSED(participant)
}

void ParticipantModel::remove(UserId user)
{
    Q_UNUSED(user)
}

void ParticipantModel::setSpeaking(UserId user, bool speaking)
{
    Q_UNUSED(user)
    Q_UNUSED(speaking)
}

void ParticipantModel::clear()
{
}

} // namespace qm
