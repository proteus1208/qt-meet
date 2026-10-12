#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <cstdio>

Q_LOGGING_CATEGORY(lcNet, "qm.net")
Q_LOGGING_CATEGORY(lcStorage, "qm.storage")
Q_LOGGING_CATEGORY(lcMedia, "qm.media")
Q_LOGGING_CATEGORY(lcRtc, "qm.rtc")
Q_LOGGING_CATEGORY(lcApp, "qm.app")

namespace qm {
namespace {

QMutex g_mutex;
QFile *g_file = nullptr;
QtMessageHandler g_previous = nullptr;
qint64 g_maxBytes = 10 * 1024 * 1024;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const QString line = QStringLiteral("%1 %2 %3\n")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  qFormatLogMessage(type, context, msg));

    fprintf(stderr, "%s", qPrintable(line));
    fflush(stderr);

    QMutexLocker lock(&g_mutex);
    if (g_file && g_file->isOpen()) {
        if (g_file->size() > g_maxBytes) {
            g_file->resize(0);
            g_file->seek(0);
        }
        g_file->write(line.toUtf8());
        g_file->flush();
    }
}

} // namespace

void Logger::install(const QString &logFile, qint64 maxBytes)
{
    QMutexLocker lock(&g_mutex);
    g_maxBytes = maxBytes > 0 ? maxBytes : 10 * 1024 * 1024;

    QString path = logFile;
    if (path.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        path = dir + QStringLiteral("/qtmeet.log");
    } else {
        QDir().mkpath(QFileInfo(path).absolutePath());
    }

    delete g_file;
    g_file = new QFile(path);
    g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

    if (!g_previous)
        g_previous = qInstallMessageHandler(messageHandler);
}

void Logger::uninstall()
{
    QMutexLocker lock(&g_mutex);
    if (g_previous) {
        qInstallMessageHandler(g_previous);
        g_previous = nullptr;
    }
    delete g_file;
    g_file = nullptr;
}

} // namespace qm
