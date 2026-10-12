#pragma once

#include "RequestContext.h"
#include "messages.pb.h"

namespace qm {

/// Aim: contract for every service the Dispatcher can route envelopes to.
/// handle() runs on a handler-pool thread; implementations must be thread-safe.
class IHandler
{
public:
    virtual ~IHandler() = default;

    virtual bool requiresAuth() const { return true; }
    virtual void handle(const RequestContext &context, const proto::Envelope &envelope) = 0;
};

} // namespace qm
