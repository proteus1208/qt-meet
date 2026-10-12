#include "Logger.h"

Q_LOGGING_CATEGORY(lcNet,     "qm.net")
Q_LOGGING_CATEGORY(lcStorage, "qm.storage")
Q_LOGGING_CATEGORY(lcMedia,   "qm.media")
Q_LOGGING_CATEGORY(lcRtc,     "qm.rtc")
Q_LOGGING_CATEGORY(lcApp,     "qm.app")

namespace qm {

void Logger::install(const QString &logFile, qint64 maxBytes)
{
    Q_UNUSED(logFile)
    Q_UNUSED(maxBytes)
}

void Logger::uninstall()
{
}

} // namespace qm
