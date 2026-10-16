#include "AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace qm {

bool AppConfig::load(const QString &path)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        // Sensible defaults when no ini exists yet.
        if (m_server.dataDir.isEmpty()) {
            m_server.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                               + QStringLiteral("/data");
        }
        if (m_server.certFile.isEmpty()) {
            const QString appDir = QCoreApplication::applicationDirPath();
            const QStringList candidates = {
                appDir + QStringLiteral("/certs/server.crt"),
                appDir + QStringLiteral("/../server/certs/server.crt"),
                QStringLiteral("D:/New folder/server/certs/server.crt"),
            };
            for (const QString &crt : candidates) {
                if (QFileInfo::exists(crt)) {
                    m_server.certFile = crt;
                    m_server.keyFile = QFileInfo(crt).absolutePath() + QStringLiteral("/server.key");
                    break;
                }
            }
        }
        if (m_client.serverHost.isEmpty())
            m_client.serverHost = QStringLiteral("127.0.0.1");
        if (m_client.caFile.isEmpty()) {
            const QString appDir = QCoreApplication::applicationDirPath();
            const QStringList candidates = {
                appDir + QStringLiteral("/certs/ca.crt"),
                appDir + QStringLiteral("/../server/certs/ca.crt"),
                QStringLiteral("D:/New folder/server/certs/ca.crt"),
            };
            for (const QString &ca : candidates) {
                if (QFileInfo::exists(ca)) {
                    m_client.caFile = ca;
                    break;
                }
            }
        }
        return false;
    }

    QSettings s(path, QSettings::IniFormat);

    s.beginGroup(QStringLiteral("server"));
    m_server.bindAddress = QHostAddress(s.value(QStringLiteral("bind"), QStringLiteral("0.0.0.0")).toString());
    m_server.tcpPort = quint16(s.value(QStringLiteral("tcpPort"), 7400).toUInt());
    m_server.httpPort = quint16(s.value(QStringLiteral("httpPort"), 7401).toUInt());
    m_server.rtcPortMin = quint16(s.value(QStringLiteral("rtcPortMin"), 40000).toUInt());
    m_server.rtcPortMax = quint16(s.value(QStringLiteral("rtcPortMax"), 40100).toUInt());
    m_server.hostedFolder = s.value(QStringLiteral("hostedFolder")).toString();
    m_server.dataDir = s.value(QStringLiteral("dataDir")).toString();
    m_server.certFile = s.value(QStringLiteral("certFile")).toString();
    m_server.keyFile = s.value(QStringLiteral("keyFile")).toString();
    m_server.netThreads = s.value(QStringLiteral("netThreads"), 0).toInt();
    m_server.handlerThreads = s.value(QStringLiteral("handlerThreads"), 0).toInt();
    m_server.transportMode =
        s.value(QStringLiteral("transportMode"), 0).toInt() == 1 ? TransportMode::Multicast
                                                                 : TransportMode::Direct;
    m_server.defaultChannelBitrateKbps =
        s.value(QStringLiteral("defaultChannelBitrateKbps"), 1200).toInt();
    s.endGroup();

    s.beginGroup(QStringLiteral("client"));
    m_client.serverHost = s.value(QStringLiteral("serverHost"), QStringLiteral("127.0.0.1")).toString();
    m_client.tcpPort = quint16(s.value(QStringLiteral("tcpPort"), 7400).toUInt());
    m_client.caFile = s.value(QStringLiteral("caFile")).toString();
    m_client.downloadDir = s.value(QStringLiteral("downloadDir")).toString();
    m_client.preferredCamera = s.value(QStringLiteral("camera")).toString();
    m_client.preferredMicrophone = s.value(QStringLiteral("microphone")).toString();
    m_client.preferredSpeaker = s.value(QStringLiteral("speaker")).toString();
    s.endGroup();

    if (m_server.dataDir.isEmpty()) {
        m_server.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + QStringLiteral("/data");
    }
    return true;
}

bool AppConfig::save(const QString &path) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSettings s(path, QSettings::IniFormat);

    s.beginGroup(QStringLiteral("server"));
    s.setValue(QStringLiteral("bind"), m_server.bindAddress.toString());
    s.setValue(QStringLiteral("tcpPort"), m_server.tcpPort);
    s.setValue(QStringLiteral("httpPort"), m_server.httpPort);
    s.setValue(QStringLiteral("rtcPortMin"), m_server.rtcPortMin);
    s.setValue(QStringLiteral("rtcPortMax"), m_server.rtcPortMax);
    s.setValue(QStringLiteral("hostedFolder"), m_server.hostedFolder);
    s.setValue(QStringLiteral("dataDir"), m_server.dataDir);
    s.setValue(QStringLiteral("certFile"), m_server.certFile);
    s.setValue(QStringLiteral("keyFile"), m_server.keyFile);
    s.setValue(QStringLiteral("netThreads"), m_server.netThreads);
    s.setValue(QStringLiteral("handlerThreads"), m_server.handlerThreads);
    s.setValue(QStringLiteral("transportMode"),
               m_server.transportMode == TransportMode::Multicast ? 1 : 0);
    s.setValue(QStringLiteral("defaultChannelBitrateKbps"), m_server.defaultChannelBitrateKbps);
    s.endGroup();

    s.beginGroup(QStringLiteral("client"));
    s.setValue(QStringLiteral("serverHost"), m_client.serverHost);
    s.setValue(QStringLiteral("tcpPort"), m_client.tcpPort);
    s.setValue(QStringLiteral("caFile"), m_client.caFile);
    s.setValue(QStringLiteral("downloadDir"), m_client.downloadDir);
    s.endGroup();

    s.sync();
    return s.status() == QSettings::NoError;
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
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + appName + QStringLiteral(".ini");
}

} // namespace qm
