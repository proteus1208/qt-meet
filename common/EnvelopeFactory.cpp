#include "EnvelopeFactory.h"

namespace qm {

quint64 EnvelopeFactory::nextSeq()
{
    return 0;
}

proto::Envelope EnvelopeFactory::request()
{
    return {};
}

proto::Envelope EnvelopeFactory::event()
{
    return {};
}

proto::Envelope EnvelopeFactory::reply(const proto::Envelope &request)
{
    Q_UNUSED(request)
    return {};
}

proto::Envelope EnvelopeFactory::error(const proto::Envelope &request,
                                       proto::ErrorCode code,
                                       const QString &message)
{
    Q_UNUSED(request)
    Q_UNUSED(code)
    Q_UNUSED(message)
    return {};
}

} // namespace qm
