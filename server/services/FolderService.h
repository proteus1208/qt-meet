#pragma once

#include <QReadWriteLock>
#include <QString>

#include "dispatch/IHandler.h"

namespace qm {

class HttpMediaServer;
class SessionRegistry;

/// Aim: browse the hosted folder. Paths are always relative to the hosted root and are
/// rejected if they escape it. Playback URLs point at HttpMediaServer.
/// Handles: DirListReq.
class FolderService : public IHandler
{
public:
    FolderService(SessionRegistry *sessions, HttpMediaServer *http);

    void handle(const RequestContext &context, const proto::Envelope &envelope) override;

    void    setRoot(const QString &absolutePath);
    QString root() const;

    /// Maps a client-relative path to an absolute path inside root; false if outside or missing.
    bool resolve(const QString &relativePath, QString &absolutePath) const;

private:
    void onDirList(const RequestContext &context, const proto::Envelope &envelope);

    SessionRegistry       *m_sessions = nullptr;
    HttpMediaServer       *m_http = nullptr;
    mutable QReadWriteLock m_lock;
    QString                m_root;
};

} // namespace qm
