#include "FolderService.h"

namespace qm {

FolderService::FolderService(SessionRegistry *sessions, HttpMediaServer *http)
    : m_sessions(sessions)
    , m_http(http)
{
}

void FolderService::handle(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

void FolderService::setRoot(const QString &absolutePath)
{
    QWriteLocker locker(&m_lock);
    m_root = absolutePath;
}

QString FolderService::root() const
{
    QReadLocker locker(&m_lock);
    return m_root;
}

bool FolderService::resolve(const QString &relativePath, QString &absolutePath) const
{
    Q_UNUSED(relativePath)
    Q_UNUSED(absolutePath)
    return false;
}

void FolderService::onDirList(const RequestContext &context, const proto::Envelope &envelope)
{
    Q_UNUSED(context)
    Q_UNUSED(envelope)
}

} // namespace qm
