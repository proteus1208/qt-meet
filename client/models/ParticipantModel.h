#pragma once

#include <QAbstractListModel>
#include <QList>

#include "common/Ids.h"
#include "messages.pb.h"

namespace qm {

/// Aim: participants of the active call / meeting for the video grid (local user first),
/// with mic / camera state and active-speaker flag.
class ParticipantModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        UserIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        AudioOnRole,
        VideoOnRole,
        SpeakingRole,
        IsLocalRole,
    };

    explicit ParticipantModel(QObject *parent = nullptr);

    int                    rowCount(const QModelIndex &parent = {}) const override;
    QVariant               data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setParticipants(const QList<proto::Participant> &participants, UserId localUser);
    void upsert(const proto::Participant &participant);
    void remove(UserId user);
    void setSpeaking(UserId user, bool speaking);
    void clear();

private:
    struct Item
    {
        proto::Participant participant;
        bool               speaking = false;
        bool               local = false;
    };

    QList<Item> m_items;
    UserId      m_localUser = 0;
};

} // namespace qm
