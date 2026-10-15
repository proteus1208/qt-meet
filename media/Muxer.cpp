#include "Muxer.h"

#include "common/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

namespace qm {

Muxer::Muxer() = default;

Muxer::~Muxer()
{
    close();
}

bool Muxer::open(const QString &url, const QString &format, const QMap<QString, QString> &options)
{
    close();
    const QByteArray urlBytes = url.toUtf8();
    const QByteArray fmtBytes = format.toUtf8();
    const int err = avformat_alloc_output_context2(&m_format, nullptr,
                                                   format.isEmpty() ? nullptr : fmtBytes.constData(),
                                                   urlBytes.constData());
    if (err < 0 || !m_format) {
        qCWarning(lcMedia) << "Muxer open failed" << errorString(err);
        return false;
    }

    for (auto it = options.cbegin(); it != options.cend(); ++it)
        av_opt_set(m_format->priv_data, it.key().toUtf8().constData(),
                   it.value().toUtf8().constData(), 0);

    if (!(m_format->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_format->pb, urlBytes.constData(), AVIO_FLAG_WRITE) < 0) {
            close();
            return false;
        }
    }
    return true;
}

int Muxer::addStream(const AVCodecParameters *parameters, int timeBaseNum, int timeBaseDen)
{
    if (!m_format || !parameters)
        return -1;
    AVStream *stream = avformat_new_stream(m_format, nullptr);
    if (!stream)
        return -1;
    if (avcodec_parameters_copy(stream->codecpar, parameters) < 0)
        return -1;
    stream->time_base = {timeBaseNum, timeBaseDen};
    return stream->index;
}

bool Muxer::writeHeader()
{
    if (!m_format || m_headerWritten)
        return m_headerWritten;
    if (avformat_write_header(m_format, nullptr) < 0)
        return false;
    m_headerWritten = true;
    return true;
}

bool Muxer::write(AVPacket *packet, int streamIndex)
{
    if (!m_format || !packet || !m_headerWritten)
        return false;
    if (streamIndex < 0 || streamIndex >= int(m_format->nb_streams))
        return false;
    packet->stream_index = streamIndex;
    AVRational srcTb = m_format->streams[streamIndex]->time_base;
    av_packet_rescale_ts(packet, srcTb, m_format->streams[streamIndex]->time_base);
    return av_interleaved_write_frame(m_format, packet) >= 0;
}

bool Muxer::finish()
{
    if (!m_format || !m_headerWritten)
        return false;
    return av_write_trailer(m_format) >= 0;
}

void Muxer::close()
{
    if (!m_format)
        return;
    if (m_format->pb && !(m_format->oformat->flags & AVFMT_NOFILE))
        avio_closep(&m_format->pb);
    avformat_free_context(m_format);
    m_format = nullptr;
    m_headerWritten = false;
}

} // namespace qm
