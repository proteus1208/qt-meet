#include "BandwidthProfile.h"

#include <QtGlobal>

namespace qm {

EncoderConfig BandwidthProfile::videoConfig() const
{
    EncoderConfig vc;
    vc.type = MediaType::Video;
    vc.videoCodec = VideoCodec::H264;
    vc.width = width;
    vc.height = height;
    vc.fps = fps;
    vc.gopFrames = gopFrames;
    vc.bitrate = videoBitrate;
    vc.hwPolicy = HwPolicy::Auto;
    vc.lowLatency = true;
    return vc;
}

EncoderConfig BandwidthProfile::audioConfig() const
{
    EncoderConfig ac;
    ac.type = MediaType::Audio;
    ac.audioCodec = AudioCodec::Opus;
    ac.sampleRate = audioSampleRate;
    ac.channels = audioChannels;
    ac.bitrate = audioBitrate;
    return ac;
}

bool BandwidthProfile::sameGeometry(const BandwidthProfile &other) const
{
    return width == other.width && height == other.height && fps == other.fps;
}

int BandwidthProfile::clampTier(int tier)
{
    return qBound(0, tier, 3);
}

BandwidthProfile BandwidthProfile::fromTier(int tier)
{
    BandwidthProfile p;
    p.tier = clampTier(tier);
    switch (p.tier) {
    case 0: // Low ~500 kbps
        p.videoBitrate = 500'000;
        p.width = 640;
        p.height = 360;
        p.fps = 10;
        p.gopFrames = 20;
        p.audioBitrate = 16'000;
        break;
    case 2: // High ~2.5 Mbps
        p.videoBitrate = 2'500'000;
        p.width = 1280;
        p.height = 720;
        p.fps = 24;
        p.gopFrames = 48;
        p.audioBitrate = 32'000;
        break;
    case 3: // Ultra ~4 Mbps
        p.videoBitrate = 4'000'000;
        p.width = 1280;
        p.height = 720;
        p.fps = 30;
        p.gopFrames = 60;
        p.audioBitrate = 48'000;
        break;
    case 1: // Medium ~1.2 Mbps
    default:
        p.videoBitrate = 1'200'000;
        p.width = 854;
        p.height = 480;
        p.fps = 15;
        p.gopFrames = 30;
        p.audioBitrate = 24'000;
        break;
    }
    p.audioSampleRate = 48000;
    p.audioChannels = 1;
    return p;
}

BandwidthProfile BandwidthProfile::fromVideoBitrateKbps(int kbps)
{
    if (kbps <= 0)
        return fromTier(1);
    if (kbps < 800)
        return fromTier(0);
    if (kbps < 1800)
        return fromTier(1);
    if (kbps < 3200)
        return fromTier(2);
    return fromTier(3);
}

} // namespace qm
