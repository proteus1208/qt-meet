#include "MediaTypes.h"

namespace qm {

FrameRef makeFrame()
{
    return {};
}

PacketRef makePacket()
{
    return {};
}

FrameRef cloneFrame(const AVFrame *frame)
{
    Q_UNUSED(frame)
    return {};
}

PacketRef clonePacket(const AVPacket *packet)
{
    Q_UNUSED(packet)
    return {};
}

QString errorString(int averror)
{
    Q_UNUSED(averror)
    return {};
}

} // namespace qm
