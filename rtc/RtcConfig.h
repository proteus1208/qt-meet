#pragma once

#include <QString>

namespace qm {

/// Aim: transport settings shared by client peers and SFU peers. No STUN/TURN: the network is
/// a routed offline LAN, so host candidates on a fixed UDP port range are sufficient.
struct RtcConfig
{
    QString bindAddress;                  // empty = all interfaces
    quint16 portRangeBegin = 40000;
    quint16 portRangeEnd   = 40100;
    int     mtu            = 1200;
    QString turnUrl;                      // optional self-hosted coturn if NAT ever appears

    int  videoBitrateMax = 2'500'000;
    int  videoBitrateMin = 150'000;
    bool simulcast       = true;          // publish a low layer for thumbnails
};

} // namespace qm
