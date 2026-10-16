#include "MediaSource.h"

#include <cstring>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>

#if defined(Q_OS_WIN)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

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

#if defined(Q_OS_WIN)
void forceLoadAvdeviceDll()
{
    // Ensure avdevice-*.dll from the exe folder is loaded (same place as ffmpeg.exe's DLLs).
    const QString dir = QCoreApplication::applicationDirPath();
    const QStringList names = QDir(dir).entryList({QStringLiteral("avdevice-*.dll")}, QDir::Files);
    for (const QString &name : names) {
        const QString path = QDir(dir).absoluteFilePath(name);
        HMODULE mod = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16()));
        if (mod) {
            using RegisterFn = void (*)();
            if (auto fn = reinterpret_cast<RegisterFn>(GetProcAddress(mod, "avdevice_register_all"))) {
                fn();
                qCInfo(lcMedia) << "Called avdevice_register_all via" << name;
            }
        } else {
            qCWarning(lcMedia) << "LoadLibrary failed for" << path << "err" << GetLastError();
        }
    }
}

const AVInputFormat *findDemuxer(const char *name)
{
    if (const AVInputFormat *fmt = av_find_input_format(name))
        return fmt;
    void *opaque = nullptr;
    const AVInputFormat *fmt = nullptr;
    while ((fmt = av_demuxer_iterate(&opaque))) {
        if (fmt->name && strcmp(fmt->name, name) == 0)
            return fmt;
    }
    return nullptr;
}

bool demuxerListed(const char *name)
{
    return findDemuxer(name) != nullptr;
}
#endif

void ensureFfmpeg()
{
    QMutexLocker locker(&g_ffmpegInitMutex);
    if (g_ffmpegInitialized)
        return;
    avformat_network_init();
#if LIBAVDEVICE_VERSION_INT < AV_VERSION_INT(59, 0, 100)
    avdevice_register_all();
#endif
#if defined(Q_OS_WIN)
    forceLoadAvdeviceDll();
    // Touch avdevice so the import lib is definitely loaded before probing.
    (void)avdevice_version();

    const bool hasGdigrab = demuxerListed("gdigrab");
    const bool hasDdagrab = demuxerListed("ddagrab");
    const bool hasDshow = demuxerListed("dshow");
    QStringList sample;
    void *opaque = nullptr;
    const AVInputFormat *fmt = nullptr;
    while ((fmt = av_demuxer_iterate(&opaque)) && sample.size() < 12) {
        if (fmt->name)
            sample << QString::fromLatin1(fmt->name);
    }
    qCInfo(lcMedia) << "FFmpeg devices: gdigrab" << hasGdigrab << "ddagrab" << hasDdagrab
                     << "dshow" << hasDshow
                     << "avformat" << avformat_version()
                     << "avdevice" << avdevice_version()
                     << "demuxers" << sample.join(QLatin1Char(','));
    if (!hasGdigrab && !hasDdagrab)
        qCWarning(lcMedia) << "No screen-grab demuxer — put FFMPEG_DIR\\bin\\av*.dll next to"
                              << QCoreApplication::applicationFilePath()
                              << "(same folder as the .exe, not the parent folder)";
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
    if (!spec.format.isEmpty()) {
        const QByteArray fmtName = spec.format.toUtf8();
#if defined(Q_OS_WIN)
        iformat = findDemuxer(fmtName.constData());
#else
        iformat = av_find_input_format(fmtName.constData());
#endif
        if (!iformat) {
            // Prefer DXGI grab when present (FFmpeg 6+), else report missing gdigrab.
            if (spec.kind == SourceKind::Screen) {
#if defined(Q_OS_WIN)
                iformat = findDemuxer("ddagrab");
#else
                iformat = av_find_input_format("ddagrab");
#endif
                if (iformat)
                    qCInfo(lcMedia) << "Using ddagrab instead of" << spec.format;
            }
            if (!iformat) {
                m_error = QStringLiteral("FFmpeg input format '%1' not found (is libavdevice/gdigrab "
                                         "in the FFmpeg DLLs next to the exe?)")
                              .arg(spec.format);
                av_dict_free(&opts);
                qCWarning(lcMedia) << "MediaSource open failed:" << m_error << spec.location;
                return false;
            }
        }
    }

    AVFormatContext *fmt = avformat_alloc_context();
    if (!fmt) {
        m_error = QStringLiteral("avformat_alloc_context failed");
        av_dict_free(&opts);
        return false;
    }
    fmt->interrupt_callback.callback = &MediaSource::interruptCallback;
    fmt->interrupt_callback.opaque = this;

    // gdigrab / ddagrab expect the device name as URL ("desktop" / "0").
    QByteArray url = spec.location.toUtf8();
    if (iformat && QString::fromLatin1(iformat->name) == QLatin1String("ddagrab")
        && (url.isEmpty() || url == "desktop"))
        url = QByteArrayLiteral("0");

    int err = avformat_open_input(&fmt, url.constData(), iformat, &opts);
    av_dict_free(&opts);
    if (err < 0) {
        m_error = errorString(err);
        avformat_free_context(fmt);
        qCWarning(lcMedia) << "MediaSource open failed:" << m_error << spec.format << spec.location;
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

namespace {

#if defined(Q_OS_WIN)
bool dshowLooksLikeAudio(const QString &name, const QString &desc)
{
    const QString hay = (name + QLatin1Char(' ') + desc).toLower();
    return hay.contains(QStringLiteral("microphone")) || hay.contains(QStringLiteral("mic "))
           || hay.contains(QStringLiteral("headset")) || hay.contains(QStringLiteral("line in"))
           || hay.contains(QStringLiteral("stereo mix")) || hay.contains(QStringLiteral("wave "))
           || hay.startsWith(QStringLiteral("audio="));
}
#endif

QList<SourceSpec> listDshowDevices(SourceKind kind)
{
    QList<SourceSpec> out;
#if defined(Q_OS_WIN)
    const AVInputFormat *fmt = av_find_input_format("dshow");
    if (!fmt)
        return out;

    AVDeviceInfoList *list = nullptr;
    if (avdevice_list_input_sources(fmt, nullptr, nullptr, &list) < 0 || !list)
        return out;

    for (int i = 0; i < list->nb_devices; ++i) {
        const AVDeviceInfo *dev = list->devices[i];
        if (!dev || !dev->device_name)
            continue;
        const QString raw = QString::fromUtf8(dev->device_name);
        const QString desc = QString::fromUtf8(dev->device_description ? dev->device_description
                                                                        : dev->device_name);
        const bool audioish = dshowLooksLikeAudio(raw, desc);
        if (kind == SourceKind::Camera && audioish)
            continue;
        if (kind == SourceKind::Microphone && !audioish)
            continue;

        SourceSpec spec;
        spec.kind = kind;
        spec.format = QStringLiteral("dshow");
        if (kind == SourceKind::Microphone) {
            spec.location = raw.startsWith(QStringLiteral("audio="))
                                ? raw
                                : (QStringLiteral("audio=") + raw);
        } else {
            spec.location = raw.startsWith(QStringLiteral("video="))
                                ? raw
                                : (QStringLiteral("video=") + raw);
        }
        spec.displayName = desc;
        out.append(spec);
    }
    avdevice_free_list_devices(&list);
#else
    Q_UNUSED(kind)
#endif
    return out;
}

} // namespace

QList<SourceSpec> MediaSource::enumerateCameras()
{
    ensureFfmpeg();
    QList<SourceSpec> out;
#if defined(Q_OS_LINUX)
    const AVInputFormat *fmt = av_find_input_format("v4l2");
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
        spec.format = QStringLiteral("v4l2");
        spec.location = QString::fromUtf8(dev->device_name);
        spec.displayName = QString::fromUtf8(dev->device_description ? dev->device_description
                                                                      : dev->device_name);
        out.append(spec);
    }
    avdevice_free_list_devices(&list);
#elif defined(Q_OS_WIN)
    out = listDshowDevices(SourceKind::Camera);
#endif
    return out;
}

QList<SourceSpec> MediaSource::enumerateMicrophones()
{
    ensureFfmpeg();
    QList<SourceSpec> out;
#if defined(Q_OS_LINUX)
    SourceSpec def;
    def.kind = SourceKind::Microphone;
    def.format = QStringLiteral("pulse");
    def.location = QStringLiteral("default");
    def.displayName = QStringLiteral("Default microphone");
    out.append(def);

    const AVInputFormat *fmt = av_find_input_format("pulse");
    if (fmt) {
        AVDeviceInfoList *list = nullptr;
        if (avdevice_list_input_sources(fmt, nullptr, nullptr, &list) >= 0 && list) {
            for (int i = 0; i < list->nb_devices; ++i) {
                const AVDeviceInfo *dev = list->devices[i];
                if (!dev)
                    continue;
                const QString name = QString::fromUtf8(dev->device_name);
                if (name == QLatin1String("default"))
                    continue;
                SourceSpec spec;
                spec.kind = SourceKind::Microphone;
                spec.format = QStringLiteral("pulse");
                spec.location = name;
                spec.displayName = QString::fromUtf8(dev->device_description
                                                         ? dev->device_description
                                                         : dev->device_name);
                out.append(spec);
            }
            avdevice_free_list_devices(&list);
        }
    }
#elif defined(Q_OS_WIN)
    out = listDshowDevices(SourceKind::Microphone);
#endif
    return out;
}

QList<SourceSpec> MediaSource::enumerateScreens()
{
    ensureFfmpeg();
    QList<SourceSpec> out;
    SourceSpec primary = defaultScreen();
    if (!primary.location.isEmpty())
        out.append(primary);
    return out;
}

SourceSpec MediaSource::defaultCamera()
{
    const QList<SourceSpec> cams = enumerateCameras();
    if (!cams.isEmpty())
        return cams.first();
#if defined(Q_OS_LINUX)
    SourceSpec cam;
    cam.kind = SourceKind::Camera;
    cam.format = QStringLiteral("v4l2");
    cam.location = QStringLiteral("/dev/video0");
    cam.displayName = QStringLiteral("Camera");
    return cam;
#elif defined(Q_OS_WIN)
    SourceSpec cam;
    cam.kind = SourceKind::Camera;
    cam.format = QStringLiteral("dshow");
    cam.displayName = QStringLiteral("Camera");
    return cam;
#else
    return {};
#endif
}

SourceSpec MediaSource::defaultMicrophone()
{
    const QList<SourceSpec> mics = enumerateMicrophones();
    if (!mics.isEmpty())
        return mics.first();
#if defined(Q_OS_LINUX)
    SourceSpec mic;
    mic.kind = SourceKind::Microphone;
    mic.format = QStringLiteral("pulse");
    mic.location = QStringLiteral("default");
    mic.displayName = QStringLiteral("Default microphone");
    return mic;
#elif defined(Q_OS_WIN)
    SourceSpec mic;
    mic.kind = SourceKind::Microphone;
    mic.format = QStringLiteral("dshow");
    mic.displayName = QStringLiteral("Microphone");
    return mic;
#else
    return {};
#endif
}

SourceSpec MediaSource::defaultScreen()
{
    ensureFfmpeg();
    SourceSpec screen;
    screen.kind = SourceKind::Screen;
    screen.displayName = QStringLiteral("Entire screen");
#if defined(Q_OS_WIN)
    // Prefer gdigrab; fall back to ddagrab (DXGI) on newer FFmpeg builds.
    if (findDemuxer("gdigrab")) {
        screen.format = QStringLiteral("gdigrab");
        screen.location = QStringLiteral("desktop");
    } else if (findDemuxer("ddagrab")) {
        screen.format = QStringLiteral("ddagrab");
        screen.location = QStringLiteral("0");
    } else {
        screen.format = QStringLiteral("gdigrab");
        screen.location = QStringLiteral("desktop");
    }
    screen.options.insert(QStringLiteral("framerate"), QStringLiteral("15"));
#elif defined(Q_OS_LINUX)
    screen.format = QStringLiteral("x11grab");
    const QByteArray display = qgetenv("DISPLAY");
    screen.location = display.isEmpty() ? QStringLiteral(":0.0") : QString::fromLocal8Bit(display);
    screen.options.insert(QStringLiteral("framerate"), QStringLiteral("15"));
#else
    return {};
#endif
    return screen;
}

QList<SourceSpec> MediaSource::enumerateSystemAudio()
{
    ensureFfmpeg();
    QList<SourceSpec> out;
#if defined(Q_OS_LINUX)
    SourceSpec monitor;
    monitor.kind = SourceKind::Microphone;
    monitor.format = QStringLiteral("pulse");
    monitor.location = QStringLiteral("default.monitor");
    monitor.displayName = QStringLiteral("System output (speakers)");
    out.append(monitor);

    const AVInputFormat *fmt = av_find_input_format("pulse");
    if (fmt) {
        AVDeviceInfoList *list = nullptr;
        if (avdevice_list_input_sources(fmt, nullptr, nullptr, &list) >= 0 && list) {
            for (int i = 0; i < list->nb_devices; ++i) {
                const AVDeviceInfo *dev = list->devices[i];
                if (!dev || !dev->device_name)
                    continue;
                const QString name = QString::fromUtf8(dev->device_name);
                if (!name.endsWith(QLatin1String(".monitor")) || name == QLatin1String("default.monitor"))
                    continue;
                SourceSpec spec;
                spec.kind = SourceKind::Microphone;
                spec.format = QStringLiteral("pulse");
                spec.location = name;
                const QString desc = QString::fromUtf8(dev->device_description
                                                           ? dev->device_description
                                                           : dev->device_name);
                spec.displayName = QStringLiteral("System output — %1").arg(desc);
                out.append(spec);
            }
            avdevice_free_list_devices(&list);
        }
    }
#elif defined(Q_OS_WIN)
    // Preferred: WASAPI loopback captures whatever the speakers are playing.
    if (av_find_input_format("wasapi")) {
        SourceSpec loopback;
        loopback.kind = SourceKind::Microphone;
        loopback.format = QStringLiteral("wasapi");
        loopback.location = QStringLiteral("default");
        loopback.options.insert(QStringLiteral("loopback"), QStringLiteral("1"));
        loopback.displayName = QStringLiteral("System output (speakers)");
        out.append(loopback);
    }

    // Also surface classic loopback / virtual-cable dshow devices when present.
    const QList<SourceSpec> mics = enumerateMicrophones();
    for (const SourceSpec &mic : mics) {
        const QString hay = (mic.displayName + QLatin1Char(' ') + mic.location).toLower();
        if (hay.contains(QStringLiteral("stereo mix")) || hay.contains(QStringLiteral("wave out"))
            || hay.contains(QStringLiteral("what u hear")) || hay.contains(QStringLiteral("loopback"))
            || hay.contains(QStringLiteral("cable")) || hay.contains(QStringLiteral("vb-audio"))) {
            SourceSpec sys = mic;
            sys.displayName = QStringLiteral("System output — %1").arg(mic.displayName);
            out.append(sys);
        }
    }
#endif
    return out;
}

SourceSpec MediaSource::defaultSystemAudio()
{
    const QList<SourceSpec> list = enumerateSystemAudio();
    if (!list.isEmpty())
        return list.first();
    return defaultMicrophone();
}

} // namespace qm
