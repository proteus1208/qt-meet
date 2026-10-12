#include "Ids.h"

#include <QAtomicInteger>
#include <QRandomGenerator>
#include <QUuid>

namespace qm {

namespace {
QAtomicInteger<quint64> g_connId{1};
}

ConnId Ids::nextConnId()
{
    return g_connId.fetchAndAddRelaxed(1);
}

QString Ids::uuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString Ids::meetingCode(int length)
{
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    QString code;
    code.reserve(length + 1);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < length; ++i) {
        if (i == length / 2)
            code.append(QLatin1Char('-'));
        code.append(QLatin1Char(alphabet[rng->bounded(int(sizeof(alphabet) - 1))]));
    }
    return code;
}

} // namespace qm
