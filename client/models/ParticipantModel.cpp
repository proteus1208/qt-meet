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
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const Item &item = m_items.at(index.row());
    const auto &user = item.participant.user();
    switch (role) {
    case UserIdRole:
        return qint64(user.id());
    case Qt::DisplayRole:
    case DisplayNameRole:
        return QString::fromStdString(user.display_name());
    case AudioOnRole:
        return item.participant.audio_on();
    case VideoOnRole:
        return item.participant.video_on();
    case SpeakingRole:
        return item.speaking;
    case IsLocalRole:
        return item.local;
    default:
        return {};
    }
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
    beginResetModel();
    m_localUser = localUser;
    m_items.clear();
    for (const proto::Participant &p : participants) {
        Item item;
        item.participant = p;
        item.local = (p.user().id() == localUser);
        if (item.local)
            m_items.prepend(item);
        else
            m_items.append(item);
    }
    endResetModel();
}

void ParticipantModel::upsert(const proto::Participant &participant)
{
    const UserId id = participant.user().id();
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items[row].participant.user().id() != id)
            continue;
        m_items[row].participant = participant;
        const QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx);
        return;
    }
    Item item;
    item.participant = participant;
    item.local = (id == m_localUser);
    const int row = item.local ? 0 : int(m_items.size());
    beginInsertRows({}, row, row);
    m_items.insert(row, item);
    endInsertRows();
}

void ParticipantModel::remove(UserId user)
{
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items[row].participant.user().id() != user)
            continue;
        beginRemoveRows({}, row, row);
        m_items.removeAt(row);
        endRemoveRows();
        return;
    }
}

void ParticipantModel::setSpeaking(UserId user, bool speaking)
{
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items[row].participant.user().id() != user)
            continue;
        if (m_items[row].speaking == speaking)
            return;
        m_items[row].speaking = speaking;
        const QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, {SpeakingRole});
        return;
    }
}

void ParticipantModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    endResetModel();
}

} // namespace qm
