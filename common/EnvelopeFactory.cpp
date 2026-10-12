#include "EnvelopeFactory.h"

#include <QAtomicInteger>

namespace qm {

namespace {
QAtomicInteger<quint64> g_seq{1};
}

quint64 EnvelopeFactory::nextSeq()
{
    return g_seq.fetchAndAddRelaxed(1);
}

proto::Envelope EnvelopeFactory::request()
{
    proto::Envelope env;
    env.set_seq(nextSeq());
    env.set_reply_to(0);
    return env;
}

proto::Envelope EnvelopeFactory::event()
{
    proto::Envelope env;
    env.set_seq(nextSeq());
    env.set_reply_to(0);
    return env;
}

proto::Envelope EnvelopeFactory::reply(const proto::Envelope &request)
{
    proto::Envelope env;
    env.set_seq(nextSeq());
    env.set_reply_to(request.seq());
    return env;
}

proto::Envelope EnvelopeFactory::error(const proto::Envelope &request,
                                       proto::ErrorCode code,
                                       const QString &message)
{
    proto::Envelope env = reply(request);
    auto *err = env.mutable_error();
    err->set_code(code);
    err->set_message(message.toStdString());
    return env;
}

} // namespace qm
