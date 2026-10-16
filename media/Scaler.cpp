#include "Scaler.h"

#include <QtGlobal>
#include <cstring>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace qm {

Scaler::Scaler() = default;

Scaler::~Scaler()
{
    reset();
}

void Scaler::configure(int dstWidth, int dstHeight, int dstPixelFormat, bool preserveAspect)
{
    m_dstWidth = dstWidth;
    m_dstHeight = dstHeight;
    m_dstFormat = dstPixelFormat;
    m_preserveAspect = preserveAspect;
    if (m_context) {
        sws_freeContext(m_context);
        m_context = nullptr;
    }
}

void Scaler::fillPlane(uint8_t *data, int linesize, int width, int height, uint8_t value)
{
    if (!data || linesize <= 0 || width <= 0 || height <= 0)
        return;
    for (int y = 0; y < height; ++y)
        std::memset(data + y * linesize, value, size_t(width));
}

bool Scaler::ensureContext(int srcWidth, int srcHeight, int srcPixelFormat, int outWidth,
                           int outHeight)
{
    if (m_context && m_srcWidth == srcWidth && m_srcHeight == srcHeight
        && m_srcFormat == srcPixelFormat && m_ctxOutWidth == outWidth && m_ctxOutHeight == outHeight)
        return true;
    if (m_context) {
        sws_freeContext(m_context);
        m_context = nullptr;
    }
    m_context = sws_getContext(srcWidth, srcHeight, AVPixelFormat(srcPixelFormat), outWidth,
                               outHeight, AVPixelFormat(m_dstFormat), SWS_BILINEAR, nullptr,
                               nullptr, nullptr);
    if (!m_context)
        return false;
    m_srcWidth = srcWidth;
    m_srcHeight = srcHeight;
    m_srcFormat = srcPixelFormat;
    m_ctxOutWidth = outWidth;
    m_ctxOutHeight = outHeight;
    return true;
}

const AVFrame *Scaler::downloadIfHardware(const AVFrame *src)
{
    if (!src || !(src->format >= 0 && av_pix_fmt_desc_get(AVPixelFormat(src->format))
                                      && (av_pix_fmt_desc_get(AVPixelFormat(src->format))->flags
                                          & AV_PIX_FMT_FLAG_HWACCEL)))
        return src;

    if (!m_swFrame)
        m_swFrame = av_frame_alloc();
    if (!m_swFrame)
        return nullptr;
    av_frame_unref(m_swFrame);
    if (av_hwframe_transfer_data(m_swFrame, src, 0) < 0)
        return nullptr;
    m_swFrame->pts = src->pts;
    return m_swFrame;
}

bool Scaler::convert(const AVFrame *src, AVFrame *dst)
{
    if (!src || !dst || m_dstWidth <= 0 || m_dstHeight <= 0 || m_dstFormat < 0)
        return false;

    const AVFrame *sys = downloadIfHardware(src);
    if (!sys)
        return false;

    int outW = m_dstWidth;
    int outH = m_dstHeight;
    int offX = 0;
    int offY = 0;
    if (m_preserveAspect && sys->width > 0 && sys->height > 0) {
        const double srcAr = double(sys->width) / double(sys->height);
        const double dstAr = double(m_dstWidth) / double(m_dstHeight);
        if (srcAr > dstAr) {
            outW = m_dstWidth;
            outH = qMax(2, int(m_dstWidth / srcAr) & ~1);
            offY = ((m_dstHeight - outH) / 2) & ~1;
        } else {
            outH = m_dstHeight;
            outW = qMax(2, int(m_dstHeight * srcAr) & ~1);
            offX = ((m_dstWidth - outW) / 2) & ~1;
        }
    }

    if (!ensureContext(sys->width, sys->height, sys->format, outW, outH))
        return false;

    av_frame_unref(dst);
    dst->format = m_dstFormat;
    dst->width = m_dstWidth;
    dst->height = m_dstHeight;
    if (av_frame_get_buffer(dst, 32) < 0)
        return false;

    // Letterbox / pillarbox with black (Y=0, UV=128 for YUV420P).
    const AVPixelFormat fmt = AVPixelFormat(m_dstFormat);
    const bool yuv420 = (fmt == AV_PIX_FMT_YUV420P || fmt == AV_PIX_FMT_YUVJ420P);
    if (yuv420) {
        fillPlane(dst->data[0], dst->linesize[0], m_dstWidth, m_dstHeight, 0);
        fillPlane(dst->data[1], dst->linesize[1], m_dstWidth / 2, m_dstHeight / 2, 128);
        fillPlane(dst->data[2], dst->linesize[2], m_dstWidth / 2, m_dstHeight / 2, 128);
    }

    uint8_t *dstSlice[4] = {nullptr, nullptr, nullptr, nullptr};
    int dstLinesize[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        dstLinesize[i] = dst->linesize[i];
        if (!dst->data[i])
            continue;
        if (i == 0)
            dstSlice[i] = dst->data[i] + offY * dst->linesize[i] + offX;
        else if (yuv420)
            dstSlice[i] = dst->data[i] + (offY / 2) * dst->linesize[i] + (offX / 2);
        else
            dstSlice[i] = dst->data[i] + offY * dst->linesize[i] + offX;
    }

    const int ret = sws_scale(m_context, sys->data, sys->linesize, 0, sys->height, dstSlice,
                              dstLinesize);
    if (ret <= 0)
        return false;
    dst->pts = src->pts;
    return true;
}

void Scaler::reset()
{
    if (m_context) {
        sws_freeContext(m_context);
        m_context = nullptr;
    }
    if (m_swFrame) {
        av_frame_free(&m_swFrame);
        m_swFrame = nullptr;
    }
    m_srcWidth = m_srcHeight = 0;
    m_srcFormat = -1;
    m_ctxOutWidth = m_ctxOutHeight = 0;
}

} // namespace qm
