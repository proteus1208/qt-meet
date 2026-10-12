#include "FileTransfer.h"

#include <QFile>
#include <QNetworkAccessManager>

#include "core/NetworkClient.h"
#include "models/TransferModel.h"

namespace qm {

struct FileTransfer::Transfer
{
    QString transferId;
    QString fileId;
    QString localPath;
    quint64 size = 0;
    quint64 done = 0;
    bool    upload = true;
};

FileTransfer::FileTransfer(NetworkClient *network, TransferModel *model, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_model(model)
{
}

FileTransfer::~FileTransfer()
{
    qDeleteAll(m_transfers);
}

void FileTransfer::setHttpAccess(const QString &baseUrl, const QString &token)
{
    m_baseUrl = baseUrl;
    m_token = token;
}

QString FileTransfer::upload(const QString &localPath, ConversationId conversation, UserId toUser)
{
    Q_UNUSED(localPath)
    Q_UNUSED(conversation)
    Q_UNUSED(toUser)
    return {};
}

QString FileTransfer::download(const QString &fileId, const QString &name, const QString &destinationDir)
{
    Q_UNUSED(fileId)
    Q_UNUSED(name)
    Q_UNUSED(destinationDir)
    return {};
}

void FileTransfer::cancel(const QString &transferId)
{
    Q_UNUSED(transferId)
}

void FileTransfer::onEvent(const proto::Envelope &envelope)
{
    Q_UNUSED(envelope)
}

void FileTransfer::sendNextChunks(const QString &transferId)
{
    Q_UNUSED(transferId)
}

} // namespace qm
