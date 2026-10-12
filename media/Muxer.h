#pragma once

#include "MediaTypes.h"

namespace qm {

/// Aim: write encoded packets to a container (meeting recordings, HLS segments, file export).
class Muxer
{
    Q_DISABLE_COPY_MOVE(Muxer)

public:
    Muxer();
    ~Muxer();

    bool open(const QString &url, const QString &format = {},
              const QMap<QString, QString> &options = {});
    int  addStream(const AVCodecParameters *parameters, int timeBaseNum, int timeBaseDen);
    bool writeHeader();
    bool write(AVPacket *packet, int streamIndex);
    bool finish();
    void close();

private:
    AVFormatContext *m_format = nullptr;
    bool             m_headerWritten = false;
};

} // namespace qm
