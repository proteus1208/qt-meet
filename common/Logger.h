#pragma once

#include <QLoggingCategory>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(lcNet)
Q_DECLARE_LOGGING_CATEGORY(lcStorage)
Q_DECLARE_LOGGING_CATEGORY(lcMedia)
Q_DECLARE_LOGGING_CATEGORY(lcRtc)
Q_DECLARE_LOGGING_CATEGORY(lcApp)

namespace qm {

/// Aim: thread-safe categorized logging to console and an optional rotating file.
class Logger
{
public:
    static void install(const QString &logFile = {}, qint64 maxBytes = 10 * 1024 * 1024);
    static void uninstall();
};

} // namespace qm
