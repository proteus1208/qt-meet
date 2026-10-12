#include "FrameConverter.h"

namespace qm {

QVideoFrame FrameConverter::toVideoFrame(const AVFrame *frame)
{
    Q_UNUSED(frame)
    return {};
}

bool FrameConverter::isDirectlyMappable(int pixelFormat)
{
    Q_UNUSED(pixelFormat)
    return false;
}

} // namespace qm
