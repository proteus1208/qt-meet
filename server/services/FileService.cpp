#include "FileService.h"

namespace qm {

FileService::FileService(StorageManager *storage, SessionRegistry *sessions, const QString &blobDir)
    : m_storage(storage)
    , m_sessions(sessions)
    , m_blobDir(blobDir)
{
}

void FileService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

QString FileService::blobPathFor(const QString &sha256) const
{
    Q_UNUSED(sha256)
    return {};
}

void FileService::abortUploadsOf(ConnId conn)
{
    Q_UNUSED(conn)
}

void FileService::onOffer(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void FileService::onChunk(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void FileService::finish(const QString &transferId, Upload &upload)
{
    Q_UNUSED(transferId)
    Q_UNUSED(upload)
}

} // namespace qm
