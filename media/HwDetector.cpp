#include "HwDetector.h"

#include <QMutex>
#include <QMutexLocker>

#include "common/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}

namespace qm {

namespace {

QMutex g_mutex;
HwCaps g_caps;
bool g_probed = false;

HwDevice fromAv(enum AVHWDeviceType type)
{
    switch (type) {
    case AV_HWDEVICE_TYPE_CUDA:
        return HwDevice::Cuda;
    case AV_HWDEVICE_TYPE_QSV:
        return HwDevice::Qsv;
    case AV_HWDEVICE_TYPE_D3D11VA:
        return HwDevice::D3D11Va;
    case AV_HWDEVICE_TYPE_DXVA2:
        return HwDevice::Dxva2;
    case AV_HWDEVICE_TYPE_VAAPI:
        return HwDevice::Vaapi;
    case AV_HWDEVICE_TYPE_VDPAU:
        return HwDevice::Vdpau;
    default:
        return HwDevice::None;
    }
}

} // namespace

const HwCaps &HwDetector::caps()
{
    QMutexLocker locker(&g_mutex);
    if (!g_probed) {
        g_caps = probe();
        g_probed = true;
    }
    return g_caps;
}

HwCaps HwDetector::probe()
{
    HwCaps caps;
    for (enum AVHWDeviceType type = av_hwdevice_iterate_types(AV_HWDEVICE_TYPE_NONE);
         type != AV_HWDEVICE_TYPE_NONE; type = av_hwdevice_iterate_types(type)) {
        const HwDevice device = fromAv(type);
        if (device == HwDevice::None)
            continue;
        if (tryCreateDevice(device)) {
            caps.decodeDevices.append(device);
            caps.hasGpu = true;
        }
    }

    // Preference order for decode.
    const HwDevice prefer[] = {HwDevice::Cuda, HwDevice::Qsv, HwDevice::Vaapi, HwDevice::D3D11Va,
                               HwDevice::Dxva2, HwDevice::Vdpau};
    for (HwDevice d : prefer) {
        if (caps.decodeDevices.contains(d)) {
            caps.bestDecodeDevice = d;
            break;
        }
    }

    const QStringList h264Candidates = {
        QStringLiteral("h264_nvenc"),  QStringLiteral("h264_qsv"),  QStringLiteral("h264_amf"),
        QStringLiteral("h264_vaapi"),  QStringLiteral("h264_mf"),   QStringLiteral("libopenh264"),
        QStringLiteral("libx264"),
    };
    for (const QString &name : h264Candidates) {
        if (tryOpenEncoder(name))
            caps.h264Encoders.append(name);
    }
    if (!caps.h264Encoders.isEmpty())
        caps.bestH264Encoder = caps.h264Encoders.first();

    qCInfo(lcMedia) << "HwDetector: decode" << int(caps.bestDecodeDevice) << "encode"
                    << caps.bestH264Encoder;
    return caps;
}

HwDevice HwDetector::bestDecodeDevice(HwPolicy policy)
{
    if (policy == HwPolicy::SoftwareOnly)
        return HwDevice::None;
    const HwCaps &c = caps();
    if (policy == HwPolicy::PreferHardware || policy == HwPolicy::Auto)
        return c.bestDecodeDevice;
    return HwDevice::None;
}

QString HwDetector::bestEncoder(VideoCodec codec, HwPolicy policy)
{
    if (codec != VideoCodec::H264)
        return softwareEncoder(codec);
    if (policy == HwPolicy::SoftwareOnly)
        return softwareEncoder(codec);
    const HwCaps &c = caps();
    if (!c.bestH264Encoder.isEmpty())
        return c.bestH264Encoder;
    return softwareEncoder(codec);
}

QString HwDetector::softwareEncoder(VideoCodec codec)
{
    switch (codec) {
    case VideoCodec::H264:
        if (tryOpenEncoder(QStringLiteral("libopenh264")))
            return QStringLiteral("libopenh264");
        if (tryOpenEncoder(QStringLiteral("libx264")))
            return QStringLiteral("libx264");
        return QStringLiteral("libopenh264");
    case VideoCodec::Hevc:
        return QStringLiteral("libx265");
    case VideoCodec::Vp8:
        return QStringLiteral("libvpx");
    }
    return {};
}

QString HwDetector::deviceName(HwDevice device)
{
    switch (device) {
    case HwDevice::Cuda:
        return QStringLiteral("cuda");
    case HwDevice::Qsv:
        return QStringLiteral("qsv");
    case HwDevice::D3D11Va:
        return QStringLiteral("d3d11va");
    case HwDevice::Dxva2:
        return QStringLiteral("dxva2");
    case HwDevice::Vaapi:
        return QStringLiteral("vaapi");
    case HwDevice::Vdpau:
        return QStringLiteral("vdpau");
    default:
        return QStringLiteral("none");
    }
}

int HwDetector::toAvDeviceType(HwDevice device)
{
    switch (device) {
    case HwDevice::Cuda:
        return AV_HWDEVICE_TYPE_CUDA;
    case HwDevice::Qsv:
        return AV_HWDEVICE_TYPE_QSV;
    case HwDevice::D3D11Va:
        return AV_HWDEVICE_TYPE_D3D11VA;
    case HwDevice::Dxva2:
        return AV_HWDEVICE_TYPE_DXVA2;
    case HwDevice::Vaapi:
        return AV_HWDEVICE_TYPE_VAAPI;
    case HwDevice::Vdpau:
        return AV_HWDEVICE_TYPE_VDPAU;
    default:
        return AV_HWDEVICE_TYPE_NONE;
    }
}

bool HwDetector::tryOpenEncoder(const QString &name)
{
    const AVCodec *codec = avcodec_find_encoder_by_name(name.toUtf8().constData());
    return codec != nullptr;
}

bool HwDetector::tryCreateDevice(HwDevice device)
{
    AVBufferRef *ctx = nullptr;
    const int err = av_hwdevice_ctx_create(&ctx, AVHWDeviceType(toAvDeviceType(device)), nullptr,
                                          nullptr, 0);
    if (err < 0)
        return false;
    av_buffer_unref(&ctx);
    return true;
}

} // namespace qm
