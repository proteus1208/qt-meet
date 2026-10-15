#pragma once

#include <QByteArray>

#include "MediaTypes.h"

namespace qm {

/// Aim: audio sample-rate / layout / format conversion (swr) for playback and encoding.
class Resampler
{
    Q_DISABLE_COPY_MOVE(Resampler)

public:
    Resampler();
    ~Resampler();

    void configure(int dstSampleRate, int dstChannels, int dstSampleFormat);   // AVSampleFormat value
    bool convert(const AVFrame *src, QByteArray &out);                          // interleaved bytes
    bool convert(const AVFrame *src, AVFrame *dst);
    void reset();

private:
    bool ensureContext(const AVFrame *src);

    SwrContext *m_context = nullptr;
    int m_dstRate = 48000;
    int m_dstChannels = 2;
    int m_dstFormat = -1;
};

} // namespace qm
