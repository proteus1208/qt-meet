#pragma once

#include <QString>
#include <QtGlobal>

namespace qm {

using UserId         = quint64;
using ConnId         = quint64;
using ConversationId = quint64;
using MessageId      = quint64;

/// Aim: generate identifiers (connection ids, uuids, short human-typable meeting codes).
class Ids
{
public:
    static ConnId  nextConnId();
    static QString uuid();
    static QString meetingCode(int length = 8);
};

} // namespace qm
