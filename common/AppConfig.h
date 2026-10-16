#pragma once

#include <QHostAddress>
#include <QString>

namespace qm {

enum class TransportMode {
    Direct = 0,     // unicast SFU relay to each participant / viewer
    Multicast = 1,  // shared encode + SFU fan-out (best for many IPTV viewers)
};

struct ServerSettings
{
    QHostAddress bindAddress = QHostAddress::Any;
    quint16 tcpPort        = 7400;
    quint16 httpPort       = 7401;
    quint16 rtcPortMin     = 40000;
    quint16 rtcPortMax     = 40100;
    QString hostedFolder;
    QString dataDir;          // sqlite db + file blobs
    QString certFile;
    QString keyFile;
    int     netThreads     = 0;   // 0 = auto (cores / 2)
    int     handlerThreads = 0;   // 0 = auto (cores)
    TransportMode transportMode = TransportMode::Direct;
    int     defaultChannelBitrateKbps = 1200;
    int     defaultCallBitrateKbps = 1200;
    int     defaultMeetingBitrateKbps = 1200;
};

struct ClientSettings
{
    QString serverHost;
    quint16 tcpPort = 7400;
    QString caFile;
    QString downloadDir;
    QString preferredCamera;
    QString preferredMicrophone;
    QString preferredSpeaker;
};

/// Aim: ini-backed application settings for server and client.
class AppConfig
{
public:
    bool load(const QString &path);
    bool save(const QString &path) const;

    ServerSettings server() const;
    ClientSettings client() const;
    void setServer(const ServerSettings &settings);
    void setClient(const ClientSettings &settings);

    static QString defaultPath(const QString &appName);

private:
    ServerSettings m_server;
    ClientSettings m_client;
};

} // namespace qm
