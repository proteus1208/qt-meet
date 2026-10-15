#include "FileService.h"

#include <QDir>
#include <QFileInfo>

#include <string>

#include "common/EnvelopeFactory.h"
#include "common/Ids.h"
#include "common/Logger.h"
#include "net/SessionRegistry.h"
#include "storage/StorageManager.h"

namespace qm {

FileService::FileService(StorageManager *storage, SessionRegistry *sessions, const QString &blobDir)
    : m_storage(storage)
    , m_sessions(sessions)
    , m_blobDir(blobDir)
{
    QDir().mkpath(m_blobDir);
    QDir().mkpath(m_blobDir + QStringLiteral("/.partial"));
}

void FileService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    switch (envelope.body_case()) {
    case proto::Envelope::kFileOffer:
        onOffer(context, envelope);
        break;
    case proto::Envelope::kFileChunk:
        onChunk(context, envelope);
        break;
    default:
        break;
    }
}

QString FileService::blobPathFor(const QString &sha256) const
{
    const QString hex = sha256.trimmed().toLower();
    if (hex.size() < 4)
        return m_blobDir + QLatin1Char('/') + hex;
    return m_blobDir + QLatin1Char('/') + hex.left(2) + QLatin1Char('/') + hex.mid(2);
}

void FileService::abortUploadsOf(ConnId conn)
{
    QMutexLocker locker(&m_mutex);
    for (auto it = m_uploads.begin(); it != m_uploads.end();) {
        if (it.value()->conn != conn) {
            ++it;
            continue;
        }
        if (it.value()->file)
            it.value()->file->close();
        it = m_uploads.erase(it);
    }
}

void FileService::onOffer(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &offer = envelope.file_offer();
    const QString transferId = QString::fromStdString(offer.transfer_id());
    const QString name = QString::fromStdString(offer.name());
    const QString sha = QString::fromStdString(offer.sha256()).trimmed().toLower();
    const QString mime = QString::fromStdString(offer.mime());
    const quint64 size = offer.size();

    if (transferId.isEmpty() || name.isEmpty() || sha.size() != 64 || size == 0) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INVALID,
                                                      QStringLiteral("Invalid file offer")));
        return;
    }

    // Dedup: complete blob with the same sha already exists.
    if (auto existing = m_storage->fileBySha(sha).result(); existing && existing->complete) {
        proto::Envelope accept = EnvelopeFactory::reply(envelope);
        auto *body = accept.mutable_file_accept();
        body->set_transfer_id(transferId.toStdString());
        body->set_file_id(existing->fileId.toStdString());
        body->set_resume_offset(existing->size);
        body->set_chunk_size(ChunkSize);
        m_sessions->sendToConn(context.conn, accept);

        proto::Envelope done = EnvelopeFactory::event();
        auto *d = done.mutable_file_done();
        d->set_transfer_id(transferId.toStdString());
        d->set_file_id(existing->fileId.toStdString());
        d->set_ok(true);
        d->set_error(proto::ERR_NONE);
        m_sessions->sendToConn(context.conn, done);
        return;
    }

    const QString fileId = Ids::uuid();
    const QString partialPath =
        m_blobDir + QStringLiteral("/.partial/") + fileId;
    const QString finalPath = blobPathFor(sha);

    FileRecord record;
    record.fileId = fileId;
    record.name = name;
    record.mime = mime;
    record.sha256 = sha;
    record.blobPath = finalPath;
    record.size = size;
    record.received = 0;
    record.owner = context.user;
    record.complete = false;
    if (!m_storage->saveFile(record).result()) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INTERNAL,
                                                      QStringLiteral("Cannot register file")));
        return;
    }

    auto upload = std::make_shared<Upload>();
    upload->fileId = fileId;
    upload->conn = context.conn;
    upload->size = size;
    upload->received = 0;
    upload->expectedSha = sha;
    upload->file = std::make_unique<QFile>(partialPath);
    upload->hash = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);

    if (!upload->file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INTERNAL,
                                                      QStringLiteral("Cannot open temp file")));
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_uploads.insert(transferId, upload);
    }

    proto::Envelope accept = EnvelopeFactory::reply(envelope);
    auto *body = accept.mutable_file_accept();
    body->set_transfer_id(transferId.toStdString());
    body->set_file_id(fileId.toStdString());
    body->set_resume_offset(0);
    body->set_chunk_size(ChunkSize);
    m_sessions->sendToConn(context.conn, accept);
}

void FileService::onChunk(const RequestContext &context, const proto::Envelope &envelope)
{
    const auto &chunk = envelope.file_chunk();
    const QString transferId = QString::fromStdString(chunk.transfer_id());
    const quint64 offset = chunk.offset();
    const std::string &bytes = chunk.data();
    const QByteArray data(bytes.data(), int(bytes.size()));

    std::shared_ptr<Upload> upload;
    {
        QMutexLocker locker(&m_mutex);
        upload = m_uploads.value(transferId);
    }
    if (!upload || upload->conn != context.conn) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_NOT_FOUND,
                                                      QStringLiteral("Unknown transfer")));
        return;
    }

    if (offset != upload->received) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INVALID,
                                                      QStringLiteral("Unexpected chunk offset")));
        return;
    }
    if (upload->received + quint64(data.size()) > upload->size) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INVALID,
                                                      QStringLiteral("Chunk past end of file")));
        return;
    }

    if (upload->file->write(data) != data.size()) {
        m_sessions->sendToConn(context.conn,
                               EnvelopeFactory::error(envelope, proto::ERR_INTERNAL,
                                                      QStringLiteral("Write failed")));
        return;
    }
    upload->hash->addData(data);
    upload->received += quint64(data.size());
    m_storage->updateFileProgress(upload->fileId, upload->received, false);

    if (upload->received < upload->size) {
        proto::Envelope ack = EnvelopeFactory::reply(envelope);
        auto *body = ack.mutable_file_ack();
        body->set_transfer_id(transferId.toStdString());
        body->set_received(upload->received);
        m_sessions->sendToConn(context.conn, ack);
        return;
    }

    const bool ok = finish(transferId, *upload);
    proto::Envelope done = EnvelopeFactory::reply(envelope);
    auto *body = done.mutable_file_done();
    body->set_transfer_id(transferId.toStdString());
    body->set_file_id(upload->fileId.toStdString());
    body->set_ok(ok);
    body->set_error(ok ? proto::ERR_NONE : proto::ERR_INVALID);
    m_sessions->sendToConn(context.conn, done);
}

bool FileService::finish(const QString &transferId, Upload &upload)
{
    upload.file->flush();
    upload.file->close();

    const QString digest = QString::fromLatin1(upload.hash->result().toHex());
    const QString partialPath = upload.file->fileName();

    auto cleanup = [&] {
        QMutexLocker locker(&m_mutex);
        m_uploads.remove(transferId);
    };

    if (digest != upload.expectedSha) {
        QFile::remove(partialPath);
        m_storage->updateFileProgress(upload.fileId, upload.received, false);
        qCWarning(lcApp) << "File hash mismatch for" << transferId;
        cleanup();
        return false;
    }

    const QString finalPath = blobPathFor(upload.expectedSha);
    QDir().mkpath(QFileInfo(finalPath).absolutePath());
    if (QFile::exists(finalPath)) {
        QFile::remove(partialPath);
    } else if (!QFile::rename(partialPath, finalPath)) {
        if (!QFile::copy(partialPath, finalPath)) {
            qCWarning(lcApp) << "Cannot finalize blob" << finalPath;
            cleanup();
            return false;
        }
        QFile::remove(partialPath);
    }

    if (auto existing = m_storage->fileById(upload.fileId).result()) {
        existing->blobPath = finalPath;
        existing->received = upload.size;
        existing->complete = true;
        m_storage->saveFile(*existing);
    } else {
        m_storage->updateFileProgress(upload.fileId, upload.size, true);
    }

    cleanup();
    return true;
}

} // namespace qm
