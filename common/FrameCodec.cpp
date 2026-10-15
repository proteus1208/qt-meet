#include "FrameCodec.h"

#include <QtEndian>
#include <cstring>

namespace qm {

QByteArray FrameCodec::encode(const proto::Envelope &envelope)
{
    const std::string payload = envelope.SerializeAsString();
    if (payload.size() > MaxFrameSize)
        return {};

    QByteArray frame(4 + int(payload.size()), Qt::Uninitialized);
    qToBigEndian(quint32(payload.size()), reinterpret_cast<uchar *>(frame.data()));
    memcpy(frame.data() + 4, payload.data(), payload.size());
    return frame;
}

bool FrameCodec::feed(const QByteArray &data)
{
    if (data.isEmpty())
        return true;

    m_buffer.append(data);

    while (m_buffer.size() >= 4) {
        const quint32 length = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar *>(m_buffer.constData()));
        if (length == 0 || length > MaxFrameSize)
            return false;
        if (m_buffer.size() < qsizetype(4 + length))
            break;
        break; // next() consumes; just validate
    }
    return true;
}

bool FrameCodec::next(proto::Envelope &out)
{
    if (m_buffer.size() < 4)
        return false;

    const quint32 length = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar *>(m_buffer.constData()));
    if (length == 0 || length > MaxFrameSize)
        return false;
    if (m_buffer.size() < qsizetype(4 + length))
        return false;

    const QByteArray payload = m_buffer.mid(4, int(length));
    m_buffer.remove(0, 4 + int(length));

    if (!out.ParseFromArray(payload.constData(), payload.size())) {
        out.Clear();
        return false;
    }
    return true;
}

void FrameCodec::reset()
{
    m_buffer.clear();
}

qsizetype FrameCodec::buffered() const
{
    return m_buffer.size();
}

} // namespace qm
