#pragma once

#include <QString>

#include "messages.pb.h"

namespace qm {

/// Aim: build request / reply / event / error envelopes with unique sequence numbers.
class EnvelopeFactory
{
public:
    static quint64 nextSeq();

    static proto::Envelope request();
    static proto::Envelope event();
    static proto::Envelope reply(const proto::Envelope &request);
    static proto::Envelope error(const proto::Envelope &request,
                                 proto::ErrorCode code,
                                 const QString &message);
};

} // namespace qm
