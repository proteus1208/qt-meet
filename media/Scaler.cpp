#include "Scaler.h"

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
}

bool Scaler::convert(const AVFrame *src, AVFrame *dst)
{
    Q_UNUSED(src)
    Q_UNUSED(dst)
    return false;
}

void Scaler::reset()
{
}

bool Scaler::ensureContext(int srcWidth, int srcHeight, int srcPixelFormat)
{
    Q_UNUSED(srcWidth)
    Q_UNUSED(srcHeight)
    Q_UNUSED(srcPixelFormat)
    return false;
}

const AVFrame *Scaler::downloadIfHardware(const AVFrame *src)
{
    return src;
}

} // namespace qm
