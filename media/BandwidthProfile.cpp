#include "BandwidthProfile.h"

#include <QtGlobal>

namespace qm {

namespace {

struct Anchor
{
    int kbps;
    int width;
    int height;
    int fps;
    int audioBitrate;
};

constexpr Anchor kAnchors[] = {
    {500, 640, 360, 10, 16'000},
    {1200, 854, 480, 15, 24'000},
    {2500, 1280, 720, 24, 32'000},
    {4000, 1280, 720, 30, 48'000},
};

int lerpInt(int a, int b, double t)
{
    return int(qRound(a + (b - a) * t));
}

const Anchor &geometryAnchorForKbps(int kbps)
{
    if (kbps < 800)
        return kAnchors[0];
    if (kbps < 1800)
        return kAnchors[1];
    if (kbps < 3200)
        return kAnchors[2];
    return kAnchors[3];
}

} // namespace

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
    return fromVideoBitrateKbps(kAnchors[clampTier(tier)].kbps);
}

BandwidthProfile BandwidthProfile::fromVideoBitrateKbps(int kbps)
{
    BandwidthProfile p;
    p.audioSampleRate = 48000;
    p.audioChannels = 1;

    if (kbps <= 0)
        kbps = 1200;
    kbps = qBound(200, kbps, 8000);

    // Continuous bitrate from the slider…
    p.videoBitrate = kbps * 1000;

    // …but stable WxH/fps steps so dragging does not thrash the encoder.
    const Anchor &geo = geometryAnchorForKbps(kbps);
    p.width = geo.width;
    p.height = geo.height;
    p.fps = geo.fps;
    p.gopFrames = qMax(10, p.fps * 2);

    // Audio bitrate eases between neighboring anchors.
    if (kbps <= kAnchors[0].kbps) {
        p.audioBitrate = kAnchors[0].audioBitrate;
        p.tier = 0;
    } else if (kbps >= kAnchors[3].kbps) {
        p.audioBitrate = kAnchors[3].audioBitrate;
        p.tier = 3;
    } else {
        int hi = 1;
        while (hi < 3 && kbps > kAnchors[hi].kbps)
            ++hi;
        const Anchor &a = kAnchors[hi - 1];
        const Anchor &b = kAnchors[hi];
        const double t = double(kbps - a.kbps) / double(b.kbps - a.kbps);
        p.audioBitrate = lerpInt(a.audioBitrate, b.audioBitrate, t);
        p.tier = (t < 0.5) ? (hi - 1) : hi;
    }

    return p;
}

} // namespace qm
