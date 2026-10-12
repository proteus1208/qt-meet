#pragma once

#include <QVideoFrame>

#include "MediaTypes.h"
#include "Scaler.h"

namespace qm {

/// Aim: AVFrame -> QVideoFrame for QML VideoOutput; maps YUV420P/NV12 directly and only
/// scales when the format is not supported by QVideoFrameFormat.
class FrameConverter
{
public:
    QVideoFrame toVideoFrame(const AVFrame *frame);

    static bool isDirectlyMappable(int pixelFormat);

private:
    Scaler m_scaler;
};

} // namespace qm
