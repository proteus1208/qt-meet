#include "Resampler.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

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
    reset();
    m_dstRate = dstSampleRate;
    m_dstChannels = dstChannels;
    m_dstFormat = dstSampleFormat;
}

bool Resampler::ensureContext(const AVFrame *src)
{
    if (!src || m_dstFormat < 0)
        return false;
    if (m_context)
        return true;

#if LIBAVUTIL_VERSION_MAJOR >= 57
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, m_dstChannels);
    const int err = swr_alloc_set_opts2(&m_context, &outLayout, AVSampleFormat(m_dstFormat),
                                        m_dstRate, &src->ch_layout, AVSampleFormat(src->format),
                                        src->sample_rate, 0, nullptr);
    if (err < 0 || !m_context)
        return false;
#else
    const int64_t outLayout =
        (m_dstChannels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    m_context = swr_alloc_set_opts(nullptr, outLayout, AVSampleFormat(m_dstFormat), m_dstRate,
                                   src->channel_layout ? src->channel_layout
                                                       : av_get_default_channel_layout(src->channels),
                                   AVSampleFormat(src->format), src->sample_rate, 0, nullptr);
    if (!m_context)
        return false;
#endif
    if (swr_init(m_context) < 0) {
        reset();
        return false;
    }
    return true;
}

bool Resampler::convert(const AVFrame *src, QByteArray &out)
{
    if (!src || !ensureContext(src))
        return false;

    const int maxOut = swr_get_out_samples(m_context, src->nb_samples);
    const int bytesPerSample = av_get_bytes_per_sample(AVSampleFormat(m_dstFormat));
    out.resize(maxOut * m_dstChannels * bytesPerSample);
    uint8_t *dstData[1] = {reinterpret_cast<uint8_t *>(out.data())};
    const int got = swr_convert(m_context, dstData, maxOut,
                                const_cast<const uint8_t **>(src->extended_data), src->nb_samples);
    if (got < 0) {
        out.clear();
        return false;
    }
    out.resize(got * m_dstChannels * bytesPerSample);
    return true;
}

bool Resampler::convert(const AVFrame *src, AVFrame *dst)
{
    if (!src || !dst || !ensureContext(src))
        return false;

    av_frame_unref(dst);
    dst->format = m_dstFormat;
    dst->sample_rate = m_dstRate;
    dst->nb_samples = swr_get_out_samples(m_context, src->nb_samples);
#if LIBAVUTIL_VERSION_MAJOR >= 57
    av_channel_layout_default(&dst->ch_layout, m_dstChannels);
#else
    dst->channels = m_dstChannels;
    dst->channel_layout =
        (m_dstChannels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
#endif
    if (av_frame_get_buffer(dst, 0) < 0)
        return false;

    const int got = swr_convert(m_context, dst->extended_data, dst->nb_samples,
                                const_cast<const uint8_t **>(src->extended_data), src->nb_samples);
    if (got < 0)
        return false;
    dst->nb_samples = got;
    dst->pts = src->pts;
    return true;
}

void Resampler::reset()
{
    if (m_context) {
        swr_free(&m_context);
        m_context = nullptr;
    }
}

} // namespace qm
