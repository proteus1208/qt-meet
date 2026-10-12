#pragma once

#include "MediaTypes.h"

namespace qm {

/// Aim: unified demuxer for file, HTTP url, local camera / microphone (dshow, v4l2)
/// and RTSP IP cameras.
class MediaSource
{
    Q_DISABLE_COPY_MOVE(MediaSource)

public:
    MediaSource();
    ~MediaSource();

    bool open(const SourceSpec &spec);
    void close();
    bool isOpen() const;
    bool isLive() const;

    MediaResult readPacket(AVPacket *packet);
    bool        seek(qint64 positionUs);
    void        interrupt();                 // unblocks a pending read (network sources)

    QList<StreamInfo>  streams() const;
    int                bestStream(MediaType type) const;
    const AVCodecParameters *codecParameters(int streamIndex) const;
    qint64             durationUs() const;
    QString            lastError() const;

    static QList<SourceSpec> enumerateCameras();
    static QList<SourceSpec> enumerateMicrophones();

private:
    static int interruptCallback(void *opaque);

    AVFormatContext *m_format = nullptr;
    SourceSpec       m_spec;
    QString          m_error;
    bool             m_interrupt = false;
};

} // namespace qm
