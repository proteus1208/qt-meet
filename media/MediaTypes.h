#pragma once

#include <QList>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <memory>

extern "C" {
struct AVFrame;
struct AVPacket;
struct AVCodecParameters;
struct AVCodecContext;
struct AVFormatContext;
struct AVBufferRef;
struct SwsContext;
struct SwrContext;
}

namespace qm {

enum class MediaType   { Video, Audio, Other };
enum class MediaResult { Ok, Again, Eof, Error };

enum class HwDevice { None, Cuda, Qsv, D3D11Va, Dxva2, Vaapi, Vdpau };
enum class HwPolicy { Auto, PreferHardware, SoftwareOnly };

enum class VideoCodec { H264, Hevc, Vp8 };
enum class AudioCodec { Opus, Aac };

enum class SourceKind { File, Url, Camera, Microphone, Rtsp, Screen };

/// Describes something FFmpeg can open: a file path, http url, dshow/v4l2 device or rtsp url.
struct SourceSpec
{
    SourceKind kind = SourceKind::File;
    QString location;                   // path, url or device name
    QString format;                     // forced input format (dshow, v4l2, ...), empty = probe
    QMap<QString, QString> options;     // AVDictionary options (rtsp_transport, video_size, ...)
    QString displayName;
};

struct StreamInfo
{
    int       index = -1;
    MediaType type = MediaType::Other;
    QString   codecName;
    int       width = 0;
    int       height = 0;
    double    fps = 0.0;
    int       sampleRate = 0;
    int       channels = 0;
    qint64    durationUs = 0;
    int       timeBaseNum = 1;
    int       timeBaseDen = 1;
};

struct EncoderConfig
{
    MediaType  type = MediaType::Video;
    VideoCodec videoCodec = VideoCodec::H264;
    AudioCodec audioCodec = AudioCodec::Opus;
    HwPolicy   hwPolicy = HwPolicy::Auto;

    int width = 1280;
    int height = 720;
    int fps = 30;
    int gopFrames = 60;
    int bitrate = 1'500'000;
    bool lowLatency = true;

    int sampleRate = 48000;
    int channels = 1;
};

struct HwCaps
{
    QList<HwDevice> decodeDevices;
    QStringList     h264Encoders;       // available, in preference order
    HwDevice        bestDecodeDevice = HwDevice::None;
    QString         bestH264Encoder;    // e.g. h264_nvenc, h264_qsv, libopenh264
    bool            hasGpu = false;
};

using FrameRef  = std::shared_ptr<AVFrame>;
using PacketRef = std::shared_ptr<AVPacket>;

FrameRef  makeFrame();
PacketRef makePacket();
FrameRef  cloneFrame(const AVFrame *frame);
PacketRef clonePacket(const AVPacket *packet);

QString errorString(int averror);

} // namespace qm

Q_DECLARE_METATYPE(qm::FrameRef)
Q_DECLARE_METATYPE(qm::PacketRef)
