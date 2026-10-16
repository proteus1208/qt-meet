#pragma once

#include "MediaTypes.h"

namespace qm {

/// Aim: resize + pixel-format conversion (sws_scale); downloads hardware frames to system
/// memory first when needed. Optionally letterboxes to preserve aspect ratio.
class Scaler
{
    Q_DISABLE_COPY_MOVE(Scaler)

public:
    Scaler();
    ~Scaler();

    void configure(int dstWidth, int dstHeight, int dstPixelFormat, bool preserveAspect = true);
    bool convert(const AVFrame *src, AVFrame *dst);
    void reset();

private:
    bool ensureContext(int srcWidth, int srcHeight, int srcPixelFormat, int outWidth, int outHeight);
    const AVFrame *downloadIfHardware(const AVFrame *src);
    static void fillPlane(uint8_t *data, int linesize, int width, int height, uint8_t value);

    SwsContext *m_context = nullptr;
    AVFrame    *m_swFrame = nullptr;
    int m_srcWidth = 0, m_srcHeight = 0, m_srcFormat = -1;
    int m_ctxOutWidth = 0, m_ctxOutHeight = 0;
    int m_dstWidth = 0, m_dstHeight = 0, m_dstFormat = -1;
    bool m_preserveAspect = true;
};

} // namespace qm
