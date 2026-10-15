#include "Scaler.h"

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

void Scaler::configure(int dstWidth, int dstHeight, int dstPixelFormat)
{
    m_dstWidth = dstWidth;
    m_dstHeight = dstHeight;
    m_dstFormat = dstPixelFormat;
    if (m_context) {
        sws_freeContext(m_context);
        m_context = nullptr;
    }
}

bool Scaler::ensureContext(int srcWidth, int srcHeight, int srcPixelFormat)
{
    if (m_context && m_srcWidth == srcWidth && m_srcHeight == srcHeight
        && m_srcFormat == srcPixelFormat)
        return true;
    if (m_context) {
        sws_freeContext(m_context);
        m_context = nullptr;
    }
    m_context = sws_getContext(srcWidth, srcHeight, AVPixelFormat(srcPixelFormat), m_dstWidth,
                               m_dstHeight, AVPixelFormat(m_dstFormat), SWS_BILINEAR, nullptr,
                               nullptr, nullptr);
    if (!m_context)
        return false;
    m_srcWidth = srcWidth;
    m_srcHeight = srcHeight;
    m_srcFormat = srcPixelFormat;
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

    if (!ensureContext(sys->width, sys->height, sys->format))
        return false;

    av_frame_unref(dst);
    dst->format = m_dstFormat;
    dst->width = m_dstWidth;
    dst->height = m_dstHeight;
    if (av_frame_get_buffer(dst, 32) < 0)
        return false;

    const int ret = sws_scale(m_context, sys->data, sys->linesize, 0, sys->height, dst->data,
                              dst->linesize);
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
}

} // namespace qm
