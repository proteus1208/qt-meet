#pragma once

#include "MediaTypes.h"

namespace qm {

/// Aim: consumer of encoded packets (RTC sender, muxer, network transfer).
class IPacketSink
{
public:
    virtual ~IPacketSink() = default;
    virtual void onPacket(const PacketRef &packet, MediaType type) = 0;
};

} // namespace qm
