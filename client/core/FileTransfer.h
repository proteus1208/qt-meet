#pragma once

#include <QHash>
#include <QObject>
#include <QUrl>

#include "common/Ids.h"
#include "messages.pb.h"

class QNetworkAccessManager;

namespace qm {

class NetworkClient;
class TransferModel;

/// Aim: chat file sharing. Uploads go over the control connection in chunks (FileOffer /
/// FileChunk, resumable from FileAccept.resume_offset); downloads use HTTP with the session
/// token. Progress is published to TransferModel.
class FileTransfer : public QObject
{
    Q_OBJECT

public:
    FileTransfer(NetworkClient *network, TransferModel *model, QObject *parent = nullptr);
    ~FileTransfer() override;

    void setHttpAccess(const QString &baseUrl, const QString &token);

    QString upload(const QString &localPath, ConversationId conversation, UserId toUser);
    QString download(const QString &fileId, const QString &name, const QString &destinationDir);
    void    cancel(const QString &transferId);

Q_SIGNALS:
    void progress(const QString &transferId, quint64 done, quint64 total);
    void uploaded(const QString &transferId, const qm::proto::ChatAttachment &attachment);
    void downloaded(const QString &transferId, const QString &localPath);
    void failed(const QString &transferId, const QString &reason);

private:
    struct Transfer;

    void onEvent(const proto::Envelope &envelope);
    void sendNextChunks(const QString &transferId);

    NetworkClient          *m_network = nullptr;
    TransferModel          *m_model = nullptr;
    QNetworkAccessManager  *m_http = nullptr;
    QString                 m_baseUrl;
    QString                 m_token;
    QHash<QString, Transfer *> m_transfers;
};

} // namespace qm
