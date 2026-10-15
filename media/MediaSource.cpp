#include "MediaSource.h"

#include <QMutex>
#include <QMutexLocker>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
}

#include "common/Logger.h"

namespace qm {

namespace {

QMutex g_ffmpegInitMutex;
bool g_ffmpegInitialized = false;

void ensureFfmpeg()
{
    QMutexLocker locker(&g_ffmpegInitMutex);
    if (g_ffmpegInitialized)
        return;
    avformat_network_init();
#if LIBAVDEVICE_VERSION_INT < AV_VERSION_INT(59, 0, 100)
    avdevice_register_all();
#endif
    g_ffmpegInitialized = true;
}

SourceKind kindFromUrl(const QString &location)
{
    if (location.startsWith(QStringLiteral("rtsp://"), Qt::CaseInsensitive))
        return SourceKind::Rtsp;
    if (location.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || location.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive))
        return SourceKind::Url;
    return SourceKind::File;
}

} // namespace

MediaSource::MediaSource() = default;

MediaSource::~MediaSource()
{
    close();
}

bool MediaSource::open(const SourceSpec &spec)
{
    ensureFfmpeg();
    close();
    m_spec = spec;
    m_interrupt = false;
    m_error.clear();

    AVDictionary *opts = nullptr;
    for (auto it = spec.options.cbegin(); it != spec.options.cend(); ++it)
        av_dict_set(&opts, it.key().toUtf8().constData(), it.value().toUtf8().constData(), 0);

    if (spec.kind == SourceKind::Rtsp || kindFromUrl(spec.location) == SourceKind::Rtsp) {
        av_dict_set(&opts, "rtsp_transport", "tcp", 0);
        av_dict_set(&opts, "stimeout", "5000000", 0);
    }
    if (spec.kind == SourceKind::Url || kindFromUrl(spec.location) == SourceKind::Url)
        av_dict_set(&opts, "reconnect", "1", 0);

    const AVInputFormat *iformat = nullptr;
    if (!spec.format.isEmpty())
        iformat = av_find_input_format(spec.format.toUtf8().constData());

    AVFormatContext *fmt = avformat_alloc_context();
    if (!fmt) {
        m_error = QStringLiteral("avformat_alloc_context failed");
        av_dict_free(&opts);
        return false;
    }
    fmt->interrupt_callback.callback = &MediaSource::interruptCallback;
    fmt->interrupt_callback.opaque = this;

    const QByteArray url = spec.location.toUtf8();
    int err = avformat_open_input(&fmt, url.constData(), iformat, &opts);
    av_dict_free(&opts);
    if (err < 0) {
        m_error = errorString(err);
        avformat_free_context(fmt);
        qCWarning(lcMedia) << "MediaSource open failed:" << m_error << spec.location;
        return false;
    }

    err = avformat_find_stream_info(fmt, nullptr);
    if (err < 0) {
        m_error = errorString(err);
        avformat_close_input(&fmt);
        return false;
    }

    m_format = fmt;
    qCInfo(lcMedia) << "Opened source" << spec.location << "streams" << m_format->nb_streams;
    return true;
}

void MediaSource::close()
{
    if (!m_format)
        return;
    avformat_close_input(&m_format);
    m_format = nullptr;
}

bool MediaSource::isOpen() const
{
    return m_format != nullptr;
}

bool MediaSource::isLive() const
{
    if (!m_format)
        return false;
    if (m_spec.kind == SourceKind::Camera || m_spec.kind == SourceKind::Microphone
        || m_spec.kind == SourceKind::Rtsp || m_spec.kind == SourceKind::Screen)
        return true;
    return m_format->duration <= 0;
}

MediaResult MediaSource::readPacket(AVPacket *packet)
{
    if (!m_format || !packet)
        return MediaResult::Error;
    av_packet_unref(packet);
    const int err = av_read_frame(m_format, packet);
    if (err == AVERROR_EOF)
        return MediaResult::Eof;
    if (err == AVERROR(EAGAIN))
        return MediaResult::Again;
    if (err < 0) {
        m_error = errorString(err);
        return MediaResult::Error;
    }
    return MediaResult::Ok;
}

bool MediaSource::seek(qint64 positionUs)
{
    if (!m_format || isLive())
        return false;
    const int err = av_seek_frame(m_format, -1, positionUs, AVSEEK_FLAG_BACKWARD);
    if (err < 0) {
        m_error = errorString(err);
        return false;
    }
    return true;
}

void MediaSource::interrupt()
{
    m_interrupt = true;
}

QList<StreamInfo> MediaSource::streams() const
{
    QList<StreamInfo> out;
    if (!m_format)
        return out;
    for (unsigned i = 0; i < m_format->nb_streams; ++i) {
        const AVStream *st = m_format->streams[i];
        if (!st || !st->codecpar)
            continue;
        StreamInfo info;
        info.index = int(i);
        info.timeBaseNum = st->time_base.num;
        info.timeBaseDen = st->time_base.den;
        info.durationUs = (st->duration > 0)
                               ? av_rescale_q(st->duration, st->time_base, {1, AV_TIME_BASE})
                               : m_format->duration;
        const AVCodecParameters *par = st->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO) {
            info.type = MediaType::Video;
            info.width = par->width;
            info.height = par->height;
            if (st->avg_frame_rate.den)
                info.fps = av_q2d(st->avg_frame_rate);
        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO) {
            info.type = MediaType::Audio;
            info.sampleRate = par->sample_rate;
#if LIBAVCODEC_VERSION_MAJOR >= 60
            info.channels = par->ch_layout.nb_channels;
#else
            info.channels = par->channels;
#endif
        } else {
            info.type = MediaType::Other;
        }
        if (const AVCodec *codec = avcodec_find_decoder(par->codec_id))
            info.codecName = QString::fromUtf8(codec->name);
        out.append(info);
    }
    return out;
}

int MediaSource::bestStream(MediaType type) const
{
    if (!m_format)
        return -1;
    const AVMediaType avType = (type == MediaType::Video)   ? AVMEDIA_TYPE_VIDEO
                               : (type == MediaType::Audio) ? AVMEDIA_TYPE_AUDIO
                                                            : AVMEDIA_TYPE_UNKNOWN;
    return av_find_best_stream(m_format, avType, -1, -1, nullptr, 0);
}

const AVCodecParameters *MediaSource::codecParameters(int streamIndex) const
{
    if (!m_format || streamIndex < 0 || streamIndex >= int(m_format->nb_streams))
        return nullptr;
    return m_format->streams[streamIndex]->codecpar;
}

qint64 MediaSource::durationUs() const
{
    if (!m_format || m_format->duration <= 0)
        return 0;
    return m_format->duration;
}

QString MediaSource::lastError() const
{
    return m_error;
}

int MediaSource::interruptCallback(void *opaque)
{
    auto *self = static_cast<MediaSource *>(opaque);
    return self && self->m_interrupt ? 1 : 0;
}

QList<SourceSpec> MediaSource::enumerateCameras()
{
    ensureFfmpeg();
    QList<SourceSpec> out;
#if defined(Q_OS_LINUX)
    const char *fmtName = "v4l2";
#elif defined(Q_OS_WIN)
    const char *fmtName = "dshow";
#else
    const char *fmtName = nullptr;
#endif
    if (!fmtName)
        return out;

    const AVInputFormat *fmt = av_find_input_format(fmtName);
    if (!fmt)
        return out;

    AVDeviceInfoList *list = nullptr;
    if (avdevice_list_input_sources(fmt, nullptr, nullptr, &list) < 0 || !list)
        return out;

    for (int i = 0; i < list->nb_devices; ++i) {
        const AVDeviceInfo *dev = list->devices[i];
        if (!dev)
            continue;
        SourceSpec spec;
        spec.kind = SourceKind::Camera;
        spec.format = QString::fromLatin1(fmtName);
        spec.location = QString::fromUtf8(dev->device_name);
        spec.displayName = QString::fromUtf8(dev->device_description
                                                 ? dev->device_description
                                                 : dev->device_name);
        out.append(spec);
    }
    avdevice_free_list_devices(&list);
    return out;
}

QList<SourceSpec> MediaSource::enumerateMicrophones()
{
    // Same device list as cameras on many platforms; filter later by open attempt if needed.
    QList<SourceSpec> cams = enumerateCameras();
    for (SourceSpec &s : cams)
        s.kind = SourceKind::Microphone;
#if defined(Q_OS_LINUX)
    // Prefer Pulse / ALSA default.
    SourceSpec def;
    def.kind = SourceKind::Microphone;
    def.format = QStringLiteral("pulse");
    def.location = QStringLiteral("default");
    def.displayName = QStringLiteral("Default microphone");
    cams.prepend(def);
#endif
    return cams;
}

} // namespace qm
