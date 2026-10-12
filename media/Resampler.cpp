#include "Resampler.h"

namespace qm {

Resampler::Resampler() = default;

Resampler::~Resampler()
{
    reset();
}

void Resampler::configure(int dstSampleRate, int dstChannels, int dstSampleFormat)
{
    m_dstRate = dstSampleRate;
    m_dstChannels = dstChannels;
    m_dstFormat = dstSampleFormat;
}

bool Resampler::convert(const AVFrame *src, QByteArray &out)
{
    Q_UNUSED(src)
    Q_UNUSED(out)
    return false;
}

bool Resampler::convert(const AVFrame *src, AVFrame *dst)
{
    Q_UNUSED(src)
    Q_UNUSED(dst)
    return false;
}

void Resampler::reset()
{
}

bool Resampler::ensureContext(const AVFrame *src)
{
    Q_UNUSED(src)
    return false;
}

} // namespace qm
