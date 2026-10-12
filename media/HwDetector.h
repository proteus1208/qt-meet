#pragma once

#include "MediaTypes.h"

namespace qm {

/// Aim: probe hardware acceleration once (decode devices + encoders that actually open)
/// and pick the best path: NVENC > QSV/VAAPI > AMF > MediaFoundation > software.
class HwDetector
{
public:
    static const HwCaps &caps();            // cached probe result
    static HwCaps probe();

    static HwDevice bestDecodeDevice(HwPolicy policy = HwPolicy::Auto);
    static QString  bestEncoder(VideoCodec codec, HwPolicy policy = HwPolicy::Auto);
    static QString  softwareEncoder(VideoCodec codec);

    static QString  deviceName(HwDevice device);
    static int      toAvDeviceType(HwDevice device);

private:
    static bool tryOpenEncoder(const QString &name);
    static bool tryCreateDevice(HwDevice device);
};

} // namespace qm
