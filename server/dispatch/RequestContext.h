#pragma once

#include <QHostAddress>

#include "common/Ids.h"

namespace qm {

/// Aim: who sent an envelope; passed to every handler call.
struct RequestContext
{
    ConnId       conn = 0;
    UserId       user = 0;          // 0 until login succeeds
    QHostAddress peer;

    bool authenticated() const { return user != 0; }
};

} // namespace qm
