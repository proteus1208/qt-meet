#include "FrameCodec.h"

namespace qm {

QByteArray FrameCodec::encode(const proto::Envelope &envelope)
{
    Q_UNUSED(envelope)
    return {};
}

bool FrameCodec::feed(const QByteArray &data)
{
    Q_UNUSED(data)
    return true;
}

bool FrameCodec::next(proto::Envelope &out)
{
    Q_UNUSED(out)
    return false;
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
