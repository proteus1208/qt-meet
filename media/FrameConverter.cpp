#include "FrameConverter.h"

#include <cstring>

#include <QVideoFrameFormat>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
}

namespace qm {

bool FrameConverter::isDirectlyMappable(int pixelFormat)
{
    switch (pixelFormat) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_BGRA:
    case AV_PIX_FMT_RGBA:
        return true;
    default:
        return false;
    }
}

static QVideoFrameFormat::PixelFormat toQtFormat(int avFormat)
{
    switch (avFormat) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
        return QVideoFrameFormat::Format_YUV420P;
    case AV_PIX_FMT_NV12:
        return QVideoFrameFormat::Format_NV12;
    case AV_PIX_FMT_BGRA:
        return QVideoFrameFormat::Format_BGRA8888;
    case AV_PIX_FMT_RGBA:
        return QVideoFrameFormat::Format_RGBA8888;
    default:
        return QVideoFrameFormat::Format_Invalid;
    }
}

static void copyPlane(uchar *dst, int dstStride, const uint8_t *src, int srcStride, int height,
                      int bytesPerLine)
{
    for (int y = 0; y < height; ++y) {
        memcpy(dst + y * dstStride, src + y * srcStride, size_t(bytesPerLine));
    }
}

QVideoFrame FrameConverter::toVideoFrame(const AVFrame *frame)
{
    if (!frame || frame->width <= 0 || frame->height <= 0)
        return {};

    const AVFrame *src = frame;
    FrameRef converted;
    int format = frame->format;

    // Download / convert HW or exotic formats to YUV420P.
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(AVPixelFormat(format));
    const bool hw = desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL);
    if (hw || !isDirectlyMappable(format)) {
        m_scaler.configure(frame->width, frame->height, AV_PIX_FMT_YUV420P);
        converted = makeFrame();
        if (!converted || !m_scaler.convert(frame, converted.get()))
            return {};
        src = converted.get();
        format = AV_PIX_FMT_YUV420P;
    }

    const QVideoFrameFormat::PixelFormat qtFmt = toQtFormat(format);
    if (qtFmt == QVideoFrameFormat::Format_Invalid)
        return {};

    QVideoFrameFormat vfFormat(QSize(src->width, src->height), qtFmt);
    QVideoFrame videoFrame(vfFormat);
    if (!videoFrame.map(QVideoFrame::WriteOnly))
        return {};

    if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUVJ420P) {
        copyPlane(videoFrame.bits(0), videoFrame.bytesPerLine(0), src->data[0], src->linesize[0],
                  src->height, src->width);
        copyPlane(videoFrame.bits(1), videoFrame.bytesPerLine(1), src->data[1], src->linesize[1],
                  src->height / 2, src->width / 2);
        copyPlane(videoFrame.bits(2), videoFrame.bytesPerLine(2), src->data[2], src->linesize[2],
                  src->height / 2, src->width / 2);
    } else if (format == AV_PIX_FMT_NV12) {
        copyPlane(videoFrame.bits(0), videoFrame.bytesPerLine(0), src->data[0], src->linesize[0],
                  src->height, src->width);
        copyPlane(videoFrame.bits(1), videoFrame.bytesPerLine(1), src->data[1], src->linesize[1],
                  src->height / 2, src->width);
    } else {
        const int bpp = (format == AV_PIX_FMT_BGRA || format == AV_PIX_FMT_RGBA) ? 4 : 3;
        copyPlane(videoFrame.bits(0), videoFrame.bytesPerLine(0), src->data[0], src->linesize[0],
                  src->height, src->width * bpp);
    }

    videoFrame.unmap();
    if (src->pts != AV_NOPTS_VALUE)
        videoFrame.setStartTime(src->pts);
    return videoFrame;
}

} // namespace qm
