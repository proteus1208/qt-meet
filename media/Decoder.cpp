#include "Decoder.h"

#include "HwDetector.h"
#include "common/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

namespace qm {

namespace {

enum AVHWDeviceType toAv(HwDevice device)
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

const AVCodec *findCodec(VideoCodec codec)
{
    switch (codec) {
    case VideoCodec::H264:
        return avcodec_find_decoder(AV_CODEC_ID_H264);
    case VideoCodec::Hevc:
        return avcodec_find_decoder(AV_CODEC_ID_HEVC);
    case VideoCodec::Vp8:
        return avcodec_find_decoder(AV_CODEC_ID_VP8);
    }
    return nullptr;
}

} // namespace

Decoder::Decoder() = default;

Decoder::~Decoder()
{
    close();
}

bool Decoder::open(const AVCodecParameters *parameters, HwPolicy policy)
{
    close();
    if (!parameters)
        return false;

    const AVCodec *codec = avcodec_find_decoder(parameters->codec_id);
    if (!codec) {
        qCWarning(lcMedia) << "No decoder for codec id" << parameters->codec_id;
        return false;
    }

    m_context = avcodec_alloc_context3(codec);
    if (!m_context)
        return false;
    if (avcodec_parameters_to_context(m_context, parameters) < 0) {
        close();
        return false;
    }

    m_type = (parameters->codec_type == AVMEDIA_TYPE_VIDEO)   ? MediaType::Video
             : (parameters->codec_type == AVMEDIA_TYPE_AUDIO) ? MediaType::Audio
                                                            : MediaType::Other;

    if (m_type == MediaType::Video && policy != HwPolicy::SoftwareOnly) {
        const HwDevice device = HwDetector::bestDecodeDevice(policy);
        if (device != HwDevice::None && setupHardware(device)) {
            m_context->opaque = this;
            m_context->get_format = &Decoder::getHwFormat;
        }
    }

    m_context->thread_count = 0; // auto
    if (avcodec_open2(m_context, codec, nullptr) < 0) {
        // Retry software-only.
        if (m_hwDevice) {
            av_buffer_unref(&m_hwDevice);
            m_hwDevice = nullptr;
            m_device = HwDevice::None;
            m_context->hw_device_ctx = nullptr;
            m_context->get_format = nullptr;
            if (avcodec_open2(m_context, codec, nullptr) < 0) {
                close();
                return false;
            }
        } else {
            close();
            return false;
        }
    }

    qCInfo(lcMedia) << "Decoder opened" << codec->name
                    << (isHardware() ? HwDetector::deviceName(m_device) : QStringLiteral("software"));
    return true;
}

bool Decoder::open(VideoCodec codec, HwPolicy policy)
{
    close();
    const AVCodec *avcodec = findCodec(codec);
    if (!avcodec)
        return false;

    AVCodecParameters *par = avcodec_parameters_alloc();
    if (!par)
        return false;
    par->codec_type = AVMEDIA_TYPE_VIDEO;
    par->codec_id = avcodec->id;
    const bool ok = open(par, policy);
    avcodec_parameters_free(&par);
    return ok;
}

bool Decoder::open(AudioCodec codec, int sampleRate, int channels)
{
    close();
    const AVCodecID id = (codec == AudioCodec::Opus) ? AV_CODEC_ID_OPUS : AV_CODEC_ID_AAC;
    const AVCodec *avcodec = avcodec_find_decoder(id);
    if (!avcodec)
        return false;

    m_context = avcodec_alloc_context3(avcodec);
    if (!m_context)
        return false;
    m_context->sample_rate = sampleRate;
#if LIBAVCODEC_VERSION_MAJOR >= 60
    av_channel_layout_default(&m_context->ch_layout, channels);
#else
    m_context->channels = channels;
    m_context->channel_layout = (channels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
#endif
    m_type = MediaType::Audio;
    if (avcodec_open2(m_context, avcodec, nullptr) < 0) {
        close();
        return false;
    }
    return true;
}

void Decoder::close()
{
    if (m_context) {
        avcodec_free_context(&m_context);
        m_context = nullptr;
    }
    if (m_hwDevice) {
        av_buffer_unref(&m_hwDevice);
        m_hwDevice = nullptr;
    }
    m_device = HwDevice::None;
    m_type = MediaType::Other;
}

enum AVPixelFormat Decoder::getHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    auto *self = static_cast<Decoder *>(ctx ? ctx->opaque : nullptr);
    const AVPixelFormat want =
        self ? AVPixelFormat(self->m_hwPixFmt) : AV_PIX_FMT_NONE;
    for (const enum AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == want)
            return *p;
    }
    // Prefer a software format over a mismatched HW surface (avoids black frames).
    for (const enum AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);
        if (desc && !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

bool Decoder::setupHardware(HwDevice device)
{
    const AVHWDeviceType type = toAv(device);
    if (type == AV_HWDEVICE_TYPE_NONE)
        return false;

    if (av_hwdevice_ctx_create(&m_hwDevice, type, nullptr, nullptr, 0) < 0) {
        m_hwDevice = nullptr;
        return false;
    }

    for (int i = 0;; ++i) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(m_context->codec, i);
        if (!config)
            break;
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX
            && config->device_type == type) {
            m_hwPixFmt = int(config->pix_fmt);
            break;
        }
    }

    m_context->hw_device_ctx = av_buffer_ref(m_hwDevice);
    m_device = device;
    return true;
}

MediaResult Decoder::send(const AVPacket *packet)
{
    if (!m_context)
        return MediaResult::Error;
    const int err = avcodec_send_packet(m_context, packet);
    if (err == AVERROR(EAGAIN))
        return MediaResult::Again;
    if (err == AVERROR_EOF)
        return MediaResult::Eof;
    if (err < 0)
        return MediaResult::Error;
    return MediaResult::Ok;
}

MediaResult Decoder::receive(AVFrame *frame)
{
    if (!m_context || !frame)
        return MediaResult::Error;
    av_frame_unref(frame);
    const int err = avcodec_receive_frame(m_context, frame);
    if (err == AVERROR(EAGAIN))
        return MediaResult::Again;
    if (err == AVERROR_EOF)
        return MediaResult::Eof;
    if (err < 0)
        return MediaResult::Error;

    // Always hand system-memory frames to callers (QVideoFrame / swscale cannot map D3D11).
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(AVPixelFormat(frame->format));
    if (desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
        AVFrame *sw = av_frame_alloc();
        if (!sw)
            return MediaResult::Error;
        if (av_hwframe_transfer_data(sw, frame, 0) < 0) {
            av_frame_free(&sw);
            qCWarning(lcMedia) << "av_hwframe_transfer_data failed";
            return MediaResult::Error;
        }
        av_frame_copy_props(sw, frame);
        av_frame_unref(frame);
        av_frame_move_ref(frame, sw);
        av_frame_free(&sw);
    }
    return MediaResult::Ok;
}

void Decoder::flush()
{
    if (m_context)
        avcodec_flush_buffers(m_context);
}

bool Decoder::isOpen() const
{
    return m_context != nullptr;
}

bool Decoder::isHardware() const
{
    return m_device != HwDevice::None;
}

HwDevice Decoder::device() const
{
    return m_device;
}

MediaType Decoder::type() const
{
    return m_type;
}

} // namespace qm
