#include "MediaTypes.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
}

namespace qm {

FrameRef makeFrame()
{
    AVFrame *frame = av_frame_alloc();
    if (!frame)
        return {};
    return FrameRef(frame, [](AVFrame *f) {
        AVFrame *tmp = f;
        av_frame_free(&tmp);
    });
}

PacketRef makePacket()
{
    AVPacket *packet = av_packet_alloc();
    if (!packet)
        return {};
    return PacketRef(packet, [](AVPacket *p) {
        AVPacket *tmp = p;
        av_packet_free(&tmp);
    });
}

FrameRef cloneFrame(const AVFrame *frame)
{
    if (!frame)
        return {};
    FrameRef copy = makeFrame();
    if (!copy)
        return {};
    if (av_frame_ref(copy.get(), frame) < 0)
        return {};
    return copy;
}

PacketRef clonePacket(const AVPacket *packet)
{
    if (!packet)
        return {};
    PacketRef copy = makePacket();
    if (!copy)
        return {};
    if (av_packet_ref(copy.get(), packet) < 0)
        return {};
    return copy;
}

QString errorString(int averror)
{
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(averror, buf, sizeof(buf));
    return QString::fromUtf8(buf);
}

} // namespace qm
