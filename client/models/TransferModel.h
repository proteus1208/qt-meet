#pragma once

#include <QAbstractListModel>
#include <QList>

namespace qm {

/// Aim: active and finished file transfers (upload / download) with progress for QML.
class TransferModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        TransferIdRole = Qt::UserRole + 1,
        NameRole,
        IsUploadRole,
        DoneRole,
        TotalRole,
        ProgressRole,
        StateRole,
        LocalPathRole,
    };

    enum class State { Queued, Running, Finished, Failed, Cancelled };
    Q_ENUM(State)

    explicit TransferModel(QObject *parent = nullptr);

    int                    rowCount(const QModelIndex &parent = {}) const override;
    QVariant               data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void add(const QString &transferId, const QString &name, quint64 total, bool upload);
    void updateProgress(const QString &transferId, quint64 done);
    void setState(const QString &transferId, State state, const QString &localPath = {});
    void removeFinished();

private:
    struct Item
    {
        QString transferId;
        QString name;
        QString localPath;
        quint64 done = 0;
        quint64 total = 0;
        bool    upload = true;
        State   state = State::Queued;
    };

    int indexOf(const QString &transferId) const;

    QList<Item> m_items;
};

} // namespace qm
