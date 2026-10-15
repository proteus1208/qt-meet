#pragma once

#include <QCryptographicHash>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QString>

#include <memory>

#include "dispatch/IHandler.h"

namespace qm {

class SessionRegistry;
class StorageManager;

/// Aim: chunked, resumable uploads into a content-addressed blob store (dedup by sha256).
/// Downloads are served by HttpMediaServer (/files/<fileId>).
/// Handles: FileOffer, FileChunk. Replies: FileAccept, FileAck, FileDone.
class FileService : public IHandler
{
public:
    static constexpr quint32 ChunkSize = 256 * 1024;

    FileService(StorageManager *storage, SessionRegistry *sessions, const QString &blobDir);

    void handle(const RequestContext &context, const proto::Envelope &envelope) override;

    QString blobPathFor(const QString &sha256) const;
    void    abortUploadsOf(ConnId conn);

private:
    struct Upload
    {
        QString                             fileId;
        ConnId                              conn = 0;
        quint64                             size = 0;
        quint64                             received = 0;
        QString                             expectedSha;
        std::unique_ptr<QFile>              file;
        std::unique_ptr<QCryptographicHash> hash;
    };

    void onOffer(const RequestContext &context, const proto::Envelope &envelope);
    void onChunk(const RequestContext &context, const proto::Envelope &envelope);
    bool finish(const QString &transferId, Upload &upload);

    StorageManager                            *m_storage = nullptr;
    SessionRegistry                           *m_sessions = nullptr;
    QString                                    m_blobDir;
    QMutex                                     m_mutex;
    QHash<QString, std::shared_ptr<Upload>>    m_uploads;     // key = transfer id
};

} // namespace qm
