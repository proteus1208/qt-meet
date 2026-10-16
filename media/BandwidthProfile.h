#pragma once

#include "MediaTypes.h"

namespace qm {

/// Aim: map a target video bitrate (kbps) to a coherent encode profile —
/// resolution, fps, GOP, and audio bitrate — so limited bandwidth stays watchable.
struct BandwidthProfile
{
    int videoBitrate = 1'200'000;
    int width = 854;
    int height = 480;
    int fps = 15;
    int gopFrames = 30;
    int audioBitrate = 24'000;
    int audioSampleRate = 48000; // Opus/WebRTC; keep fixed
    int audioChannels = 1;
    int tier = 1; // 0 Low … 3 Ultra

    EncoderConfig videoConfig() const;
    EncoderConfig audioConfig() const;

    bool sameGeometry(const BandwidthProfile &other) const;

    static BandwidthProfile fromVideoBitrateKbps(int kbps);
    static BandwidthProfile fromTier(int tier); // 0..3
    static int clampTier(int tier);
};

} // namespace qm
