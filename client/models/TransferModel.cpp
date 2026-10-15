#include "TransferModel.h"

namespace qm {

TransferModel::TransferModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TransferModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant TransferModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const Item &item = m_items.at(index.row());
    switch (role) {
    case TransferIdRole:
        return item.transferId;
    case NameRole:
        return item.name;
    case IsUploadRole:
        return item.upload;
    case DoneRole:
        return qint64(item.done);
    case TotalRole:
        return qint64(item.total);
    case ProgressRole:
        return item.total == 0 ? 0.0 : double(item.done) / double(item.total);
    case StateRole:
        return int(item.state);
    case LocalPathRole:
        return item.localPath;
    default:
        return {};
    }
}

QHash<int, QByteArray> TransferModel::roleNames() const
{
    return {
        {TransferIdRole, "transferId"},
        {NameRole, "name"},
        {IsUploadRole, "isUpload"},
        {DoneRole, "done"},
        {TotalRole, "total"},
        {ProgressRole, "progress"},
        {StateRole, "state"},
        {LocalPathRole, "localPath"},
    };
}

void TransferModel::add(const QString &transferId, const QString &name, quint64 total, bool upload)
{
    if (indexOf(transferId) >= 0)
        return;
    Item item;
    item.transferId = transferId;
    item.name = name;
    item.total = total;
    item.upload = upload;
    item.state = State::Running;
    const int row = int(m_items.size());
    beginInsertRows({}, row, row);
    m_items.append(item);
    endInsertRows();
}

void TransferModel::updateProgress(const QString &transferId, quint64 done)
{
    const int row = indexOf(transferId);
    if (row < 0)
        return;
    m_items[row].done = done;
    if (m_items[row].state == State::Queued)
        m_items[row].state = State::Running;
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx, {DoneRole, ProgressRole, StateRole});
}

void TransferModel::setState(const QString &transferId, State state, const QString &localPath)
{
    const int row = indexOf(transferId);
    if (row < 0)
        return;
    m_items[row].state = state;
    if (!localPath.isEmpty())
        m_items[row].localPath = localPath;
    if (state == State::Finished)
        m_items[row].done = m_items[row].total;
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx, {StateRole, LocalPathRole, DoneRole, ProgressRole});
}

void TransferModel::removeFinished()
{
    for (int row = m_items.size() - 1; row >= 0; --row) {
        const State state = m_items[row].state;
        if (state != State::Finished && state != State::Failed && state != State::Cancelled)
            continue;
        beginRemoveRows({}, row, row);
        m_items.removeAt(row);
        endRemoveRows();
    }
}

int TransferModel::indexOf(const QString &transferId) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].transferId == transferId)
            return i;
    }
    return -1;
}

} // namespace qm
