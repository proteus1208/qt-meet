#pragma once

#include <QByteArray>

#include "messages.pb.h"

namespace qm {

/// Aim: length-prefixed framing of serialized proto::Envelope over a byte stream
/// (uint32 big-endian length followed by the payload).
class FrameCodec
{
public:
    static constexpr quint32 MaxFrameSize = 16 * 1024 * 1024;

    static QByteArray encode(const proto::Envelope &envelope);

    /// Returns false when the stream is corrupt (oversized frame); the connection must be dropped.
    bool feed(const QByteArray &data);

    /// Pops one complete envelope if available.
    bool next(proto::Envelope &out);

    void reset();
    qsizetype buffered() const;

private:
    QByteArray m_buffer;
};

} // namespace qm
