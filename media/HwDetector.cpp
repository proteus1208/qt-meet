#include "HwDetector.h"

namespace qm {

const HwCaps &HwDetector::caps()
{
    static const HwCaps cached = probe();
    return cached;
}

HwCaps HwDetector::probe()
{
    return {};
}

HwDevice HwDetector::bestDecodeDevice(HwPolicy policy)
{
    Q_UNUSED(policy)
    return HwDevice::None;
}

QString HwDetector::bestEncoder(VideoCodec codec, HwPolicy policy)
{
    Q_UNUSED(codec)
    Q_UNUSED(policy)
    return {};
}

QString HwDetector::softwareEncoder(VideoCodec codec)
{
    Q_UNUSED(codec)
    return {};
}

QString HwDetector::deviceName(HwDevice device)
{
    Q_UNUSED(device)
    return {};
}

int HwDetector::toAvDeviceType(HwDevice device)
{
    Q_UNUSED(device)
    return 0;
}

bool HwDetector::tryOpenEncoder(const QString &name)
{
    Q_UNUSED(name)
    return false;
}

bool HwDetector::tryCreateDevice(HwDevice device)
{
    Q_UNUSED(device)
    return false;
}

} // namespace qm
