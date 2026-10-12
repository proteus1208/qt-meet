#include "Ids.h"

namespace qm {

ConnId Ids::nextConnId()
{
    return 0;
}

QString Ids::uuid()
{
    return {};
}

QString Ids::meetingCode(int length)
{
    Q_UNUSED(length)
    return {};
}

} // namespace qm
