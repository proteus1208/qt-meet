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
    Q_UNUSED(index)
    Q_UNUSED(role)
    return {};
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
    Q_UNUSED(transferId)
    Q_UNUSED(name)
    Q_UNUSED(total)
    Q_UNUSED(upload)
}

void TransferModel::updateProgress(const QString &transferId, quint64 done)
{
    Q_UNUSED(transferId)
    Q_UNUSED(done)
}

void TransferModel::setState(const QString &transferId, State state, const QString &localPath)
{
    Q_UNUSED(transferId)
    Q_UNUSED(state)
    Q_UNUSED(localPath)
}

void TransferModel::removeFinished()
{
}

int TransferModel::indexOf(const QString &transferId) const
{
    Q_UNUSED(transferId)
    return -1;
}

} // namespace qm
