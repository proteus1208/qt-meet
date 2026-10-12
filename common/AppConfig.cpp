#include "AppConfig.h"

namespace qm {

bool AppConfig::load(const QString &path)
{
    Q_UNUSED(path)
    return false;
}

bool AppConfig::save(const QString &path) const
{
    Q_UNUSED(path)
    return false;
}

ServerSettings AppConfig::server() const
{
    return m_server;
}

ClientSettings AppConfig::client() const
{
    return m_client;
}

void AppConfig::setServer(const ServerSettings &settings)
{
    m_server = settings;
}

void AppConfig::setClient(const ClientSettings &settings)
{
    m_client = settings;
}

QString AppConfig::defaultPath(const QString &appName)
{
    Q_UNUSED(appName)
    return {};
}

} // namespace qm
