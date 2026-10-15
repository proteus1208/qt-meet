#include "FileTransfer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

#include <memory>

#include "common/EnvelopeFactory.h"
#include "common/Ids.h"
#include "common/Logger.h"
#include "core/NetworkClient.h"
#include "models/TransferModel.h"

namespace qm {

namespace {
constexpr quint32 kChunkSize = 256 * 1024;
}

struct FileTransfer::Transfer
{
    QString transferId;
    QString fileId;
    QString localPath;
    QString name;
    QString mime;
    QString sha256;
    quint64 size = 0;
    quint64 done = 0;
    quint32 chunkSize = kChunkSize;
    ConversationId conversation = 0;
    UserId toUser = 0;
    bool upload = true;
    bool cancelled = false;
    std::unique_ptr<QFile> file;
};

FileTransfer::FileTransfer(NetworkClient *network, TransferModel *model, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_model(model)
    , m_http(new QNetworkAccessManager(this))
{
    connect(m_network, &NetworkClient::event, this, &FileTransfer::onEvent);
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
    QFileInfo info(localPath);
    if (!info.exists() || !info.isFile()) {
        Q_EMIT failed({}, QStringLiteral("File not found"));
        return {};
    }

    auto *transfer = new Transfer;
    transfer->transferId = Ids::uuid();
    transfer->localPath = info.absoluteFilePath();
    transfer->name = info.fileName();
    transfer->mime = QMimeDatabase().mimeTypeForFile(info).name();
    transfer->size = quint64(info.size());
    transfer->conversation = conversation;
    transfer->toUser = toUser;
    transfer->upload = true;
    transfer->file = std::make_unique<QFile>(transfer->localPath);
    if (!transfer->file->open(QIODevice::ReadOnly)) {
        delete transfer;
        Q_EMIT failed({}, QStringLiteral("Cannot open file"));
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(transfer->file.get())) {
        delete transfer;
        Q_EMIT failed({}, QStringLiteral("Cannot hash file"));
        return {};
    }
    transfer->sha256 = QString::fromLatin1(hash.result().toHex());
    transfer->file->seek(0);

    m_transfers.insert(transfer->transferId, transfer);
    m_model->add(transfer->transferId, transfer->name, transfer->size, true);
    startUpload(transfer);
    return transfer->transferId;
}

QString FileTransfer::download(const QString &fileId, const QString &name, const QString &destinationDir)
{
    if (m_baseUrl.isEmpty() || m_token.isEmpty()) {
        Q_EMIT failed({}, QStringLiteral("HTTP session not ready"));
        return {};
    }

    QString dir = destinationDir;
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (dir.isEmpty())
            dir = QDir::homePath();
    }
    QDir().mkpath(dir);

    auto *transfer = new Transfer;
    transfer->transferId = Ids::uuid();
    transfer->fileId = fileId;
    transfer->name = name.isEmpty() ? fileId : name;
    transfer->localPath = QDir(dir).filePath(transfer->name);
    transfer->upload = false;
    m_transfers.insert(transfer->transferId, transfer);
    m_model->add(transfer->transferId, transfer->name, 0, false);

    QUrl url(m_baseUrl + QStringLiteral("/files/") + QString::fromUtf8(QUrl::toPercentEncoding(fileId)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("t"), m_token);
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_http->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, id = transfer->transferId](qint64 received, qint64 total) {
                if (auto *t = m_transfers.value(id)) {
                    t->done = quint64(received);
                    t->size = total > 0 ? quint64(total) : t->size;
                    m_model->updateProgress(id, t->done);
                    Q_EMIT progress(id, t->done, t->size);
                }
            });
    connect(reply, &QNetworkReply::finished, this, [this, reply, id = transfer->transferId] {
        reply->deleteLater();
        Transfer *transfer = m_transfers.value(id);
        if (!transfer)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            fail(transfer, reply->errorString());
            return;
        }
        QFile out(transfer->localPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || out.write(reply->readAll()) < 0) {
            fail(transfer, QStringLiteral("Cannot write download"));
            return;
        }
        out.close();
        m_model->setState(id, TransferModel::State::Finished, transfer->localPath);
        Q_EMIT downloaded(id, transfer->localPath);
        m_transfers.remove(id);
        delete transfer;
    });

    return transfer->transferId;
}

void FileTransfer::cancel(const QString &transferId)
{
    Transfer *transfer = m_transfers.take(transferId);
    if (!transfer)
        return;
    transfer->cancelled = true;
    m_model->setState(transferId, TransferModel::State::Cancelled);
    delete transfer;
}

void FileTransfer::fail(Transfer *transfer, const QString &reason)
{
    if (!transfer)
        return;
    const QString id = transfer->transferId;
    qCWarning(lcNet) << "Transfer" << id << "failed:" << reason;
    m_model->setState(id, TransferModel::State::Failed);
    Q_EMIT failed(id, reason);
    m_transfers.remove(id);
    delete transfer;
}

void FileTransfer::startUpload(Transfer *transfer)
{
    proto::Envelope env = EnvelopeFactory::request();
    auto *offer = env.mutable_file_offer();
    offer->set_transfer_id(transfer->transferId.toStdString());
    offer->set_name(transfer->name.toStdString());
    offer->set_size(transfer->size);
    offer->set_sha256(transfer->sha256.toStdString());
    offer->set_mime(transfer->mime.toStdString());
    offer->set_conversation_id(transfer->conversation);
    offer->set_to_user_id(transfer->toUser);

    auto *watcher = new QFutureWatcher<proto::Envelope>(this);
    connect(watcher, &QFutureWatcher<proto::Envelope>::finished, this,
            [this, watcher, id = transfer->transferId] {
                watcher->deleteLater();
                Transfer *transfer = m_transfers.value(id);
                if (!transfer || transfer->cancelled)
                    return;
                proto::Envelope reply;
                try {
                    reply = watcher->result();
                } catch (const std::exception &ex) {
                    fail(transfer, QString::fromUtf8(ex.what()));
                    return;
                }
                if (reply.has_error()) {
                    fail(transfer, QString::fromStdString(reply.error().message()));
                    return;
                }
                if (reply.has_file_done() && reply.file_done().ok()) {
                    transfer->fileId = QString::fromStdString(reply.file_done().file_id());
                    transfer->done = transfer->size;
                    m_model->updateProgress(id, transfer->done);
                    m_model->setState(id, TransferModel::State::Finished);
                    proto::ChatAttachment att;
                    att.set_file_id(transfer->fileId.toStdString());
                    att.set_name(transfer->name.toStdString());
                    att.set_size(transfer->size);
                    att.set_mime(transfer->mime.toStdString());
                    Q_EMIT uploaded(id, att);
                    m_transfers.remove(id);
                    delete transfer;
                    return;
                }
                if (!reply.has_file_accept()) {
                    fail(transfer, QStringLiteral("Unexpected offer reply"));
                    return;
                }
                const auto &accept = reply.file_accept();
                transfer->fileId = QString::fromStdString(accept.file_id());
                transfer->done = accept.resume_offset();
                if (accept.chunk_size() > 0)
                    transfer->chunkSize = accept.chunk_size();
                if (!transfer->file->seek(qint64(transfer->done))) {
                    fail(transfer, QStringLiteral("Cannot seek file"));
                    return;
                }
                m_model->updateProgress(id, transfer->done);
                if (transfer->done >= transfer->size) {
                    // Dedup: FileDone arrives as a separate event.
                    return;
                }
                sendNextChunks(transfer);
            });
    watcher->setFuture(m_network->request(env, 30000));
}

void FileTransfer::sendNextChunks(Transfer *transfer)
{
    if (!transfer || transfer->cancelled || transfer->done >= transfer->size)
        return;

    const quint64 remaining = transfer->size - transfer->done;
    const quint64 take = qMin(remaining, quint64(transfer->chunkSize));
    const QByteArray data = transfer->file->read(qint64(take));
    if (quint64(data.size()) != take) {
        fail(transfer, QStringLiteral("Read failed"));
        return;
    }

    proto::Envelope env = EnvelopeFactory::request();
    auto *chunk = env.mutable_file_chunk();
    chunk->set_transfer_id(transfer->transferId.toStdString());
    chunk->set_offset(transfer->done);
    chunk->set_data(data.constData(), data.size());

    const QString id = transfer->transferId;
    auto *watcher = new QFutureWatcher<proto::Envelope>(this);
    connect(watcher, &QFutureWatcher<proto::Envelope>::finished, this, [this, watcher, id] {
        watcher->deleteLater();
        Transfer *transfer = m_transfers.value(id);
        if (!transfer || transfer->cancelled)
            return;
        proto::Envelope reply;
        try {
            reply = watcher->result();
        } catch (const std::exception &ex) {
            fail(transfer, QString::fromUtf8(ex.what()));
            return;
        }
        if (reply.has_error()) {
            fail(transfer, QString::fromStdString(reply.error().message()));
            return;
        }
        if (reply.has_file_ack()) {
            transfer->done = reply.file_ack().received();
            m_model->updateProgress(id, transfer->done);
            Q_EMIT progress(id, transfer->done, transfer->size);
            sendNextChunks(transfer);
            return;
        }
        if (reply.has_file_done()) {
            if (!reply.file_done().ok()) {
                fail(transfer, QStringLiteral("Upload rejected"));
                return;
            }
            transfer->fileId = QString::fromStdString(reply.file_done().file_id());
            transfer->done = transfer->size;
            m_model->updateProgress(id, transfer->done);
            m_model->setState(id, TransferModel::State::Finished);
            proto::ChatAttachment att;
            att.set_file_id(transfer->fileId.toStdString());
            att.set_name(transfer->name.toStdString());
            att.set_size(transfer->size);
            att.set_mime(transfer->mime.toStdString());
            Q_EMIT uploaded(id, att);
            m_transfers.remove(id);
            delete transfer;
            return;
        }
        fail(transfer, QStringLiteral("Unexpected chunk reply"));
    });
    watcher->setFuture(m_network->request(env, 60000));
}

void FileTransfer::onEvent(const proto::Envelope &envelope)
{
    if (!envelope.has_file_done())
        return;

    const QString id = QString::fromStdString(envelope.file_done().transfer_id());
    Transfer *transfer = m_transfers.value(id);
    if (!transfer || !transfer->upload)
        return;

    if (!envelope.file_done().ok()) {
        fail(transfer, QStringLiteral("Upload failed"));
        return;
    }

    transfer->fileId = QString::fromStdString(envelope.file_done().file_id());
    transfer->done = transfer->size;
    m_model->updateProgress(id, transfer->done);
    m_model->setState(id, TransferModel::State::Finished);
    proto::ChatAttachment att;
    att.set_file_id(transfer->fileId.toStdString());
    att.set_name(transfer->name.toStdString());
    att.set_size(transfer->size);
    att.set_mime(transfer->mime.toStdString());
    Q_EMIT uploaded(id, att);
    m_transfers.remove(id);
    delete transfer;
}

} // namespace qm
