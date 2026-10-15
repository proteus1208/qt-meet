#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>

#include <memory>

#include "admin/AdminController.h"
#include "common/AppConfig.h"
#include "common/Logger.h"
#include "ServerApp.h"

namespace {

bool hasArg(int argc, char *argv[], const char *name)
{
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], name) == 0)
            return true;
    }
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    // A GUI application cannot be created on a display-less Linux host, so decide before constructing one.
    const bool headless = hasArg(argc, argv, "--headless");

    std::unique_ptr<QCoreApplication> app;
    if (headless)
        app = std::make_unique<QCoreApplication>(argc, argv);
    else
        app = std::make_unique<QGuiApplication>(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("QtMeet"));
    QCoreApplication::setApplicationName(QStringLiteral("QtMeetServer"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("config"), QStringLiteral("Path to server ini file."), QStringLiteral("file")});
    parser.addOption({QStringLiteral("headless"), QStringLiteral("Run without the admin window.")});
    parser.process(*app);

    qm::Logger::install();

    qm::AppConfig config;
    const QString configPath = parser.isSet(QStringLiteral("config"))
                                   ? parser.value(QStringLiteral("config"))
                                   : qm::AppConfig::defaultPath(QStringLiteral("server"));
    config.load(configPath); // applies defaults when file is missing

    qm::ServerApp server(config.server());

    if (headless) {
        if (!server.start())
            return 1;
        return app->exec();
    }

    qm::AdminController admin(&server);
    qmlRegisterSingletonInstance("QtMeet.Server", 1, 0, "Admin", &admin);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     app.get(), [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/AdminMain.qml")));

    return app->exec();
}
