#include "Encoder.h"

#include "HwDetector.h"
#include "common/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

namespace qm {

Encoder::Encoder() = default;

Encoder::~Encoder()
{
    close();
}

bool Encoder::open(const EncoderConfig &config)
{
    close();
    m_config = config;
    if (config.type == MediaType::Video)
        return openVideo(HwDetector::bestEncoder(config.videoCodec, config.hwPolicy));
    if (config.type == MediaType::Audio)
        return openAudio();
    return false;
}

bool Encoder::openVideo(const QString &encoderName)
{
    const AVCodec *codec = avcodec_find_encoder_by_name(encoderName.toUtf8().constData());
    if (!codec) {
        const QString fallback = HwDetector::softwareEncoder(m_config.videoCodec);
        codec = avcodec_find_encoder_by_name(fallback.toUtf8().constData());
        if (!codec)
            codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (!codec) {
        qCWarning(lcMedia) << "No H.264 encoder available";
        return false;
    }

    m_context = avcodec_alloc_context3(codec);
    if (!m_context)
        return false;

    m_context->width = m_config.width;
    m_context->height = m_config.height;
    m_context->time_base = {1, m_config.fps > 0 ? m_config.fps : 30};
    m_context->framerate = {m_config.fps > 0 ? m_config.fps : 30, 1};
    m_context->gop_size = m_config.gopFrames;
    m_context->max_b_frames = 0;
    m_context->bit_rate = m_config.bitrate;
    m_context->pix_fmt = AV_PIX_FMT_YUV420P;
    if (m_config.lowLatency) {
        av_opt_set(m_context->priv_data, "tune", "zerolatency", 0);
        av_opt_set(m_context->priv_data, "preset", "veryfast", 0);
    }

    if (avcodec_open2(m_context, codec, nullptr) < 0) {
        close();
        return false;
    }
    m_encoderName = QString::fromUtf8(codec->name);
    qCInfo(lcMedia) << "Encoder opened" << m_encoderName;
    return true;
}

bool Encoder::openAudio()
{
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    if (!codec)
        codec = avcodec_find_encoder_by_name("libopus");
    if (!codec) {
        qCWarning(lcMedia) << "No Opus encoder available";
        return false;
    }

    m_context = avcodec_alloc_context3(codec);
    if (!m_context)
        return false;

    m_context->sample_rate = m_config.sampleRate;
    m_context->bit_rate = qMax(16'000, m_config.bitrate);
#if LIBAVCODEC_VERSION_MAJOR >= 60
    av_channel_layout_default(&m_context->ch_layout, m_config.channels);
#else
    m_context->channels = m_config.channels;
    m_context->channel_layout =
        (m_config.channels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
#endif
    m_context->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    m_context->time_base = {1, m_config.sampleRate};

    if (avcodec_open2(m_context, codec, nullptr) < 0) {
        close();
        return false;
    }
    m_encoderName = QString::fromUtf8(codec->name);
    return true;
}

void Encoder::close()
{
    if (m_context) {
        avcodec_free_context(&m_context);
        m_context = nullptr;
    }
    if (m_hwFrames) {
        av_buffer_unref(&m_hwFrames);
        m_hwFrames = nullptr;
    }
    m_encoderName.clear();
    m_forceKeyFrame = false;
}

MediaResult Encoder::send(const AVFrame *frame)
{
    if (!m_context)
        return MediaResult::Error;
    AVFrame *mutableFrame = const_cast<AVFrame *>(frame);
    if (mutableFrame && m_forceKeyFrame) {
        mutableFrame->pict_type = AV_PICTURE_TYPE_I;
        m_forceKeyFrame = false;
    }
    const int err = avcodec_send_frame(m_context, frame);
    if (err == AVERROR(EAGAIN))
        return MediaResult::Again;
    if (err == AVERROR_EOF)
        return MediaResult::Eof;
    if (err < 0)
        return MediaResult::Error;
    return MediaResult::Ok;
}

MediaResult Encoder::receive(AVPacket *packet)
{
    if (!m_context || !packet)
        return MediaResult::Error;
    av_packet_unref(packet);
    const int err = avcodec_receive_packet(m_context, packet);
    if (err == AVERROR(EAGAIN))
        return MediaResult::Again;
    if (err == AVERROR_EOF)
        return MediaResult::Eof;
    if (err < 0)
        return MediaResult::Error;
    return MediaResult::Ok;
}

void Encoder::setBitrate(int bitsPerSecond)
{
    if (!m_context)
        return;
    m_context->bit_rate = bitsPerSecond;
    m_config.bitrate = bitsPerSecond;
}

void Encoder::requestKeyFrame()
{
    m_forceKeyFrame = true;
}

bool Encoder::fillParameters(AVCodecParameters *parameters) const
{
    if (!m_context || !parameters)
        return false;
    return avcodec_parameters_from_context(parameters, m_context) >= 0;
}

int Encoder::frameSize() const
{
    return m_context ? m_context->frame_size : 0;
}

bool Encoder::isOpen() const
{
    return m_context != nullptr;
}

bool Encoder::isHardware() const
{
    return m_encoderName.contains(QStringLiteral("nvenc"))
           || m_encoderName.contains(QStringLiteral("qsv"))
           || m_encoderName.contains(QStringLiteral("amf"))
           || m_encoderName.contains(QStringLiteral("vaapi"));
}

QString Encoder::encoderName() const
{
    return m_encoderName;
}

const EncoderConfig &Encoder::config() const
{
    return m_config;
}

} // namespace qm
