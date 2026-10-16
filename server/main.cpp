#include <QCommandLineParser>
#include <QCoreApplication>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml>

#include <memory>

#include "admin/AdminController.h"
#include "common/AppConfig.h"
#include "common/BuildMode.h"
#include "common/DesktopShell.h"
#include "common/Logger.h"
#include "common/SingleInstance.h"
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
    const bool headless = hasArg(argc, argv, "--headless");

    std::unique_ptr<QCoreApplication> app;
    if (headless) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    } else {
        auto *gui = new QApplication(argc, argv);
        QApplication::setQuitOnLastWindowClosed(false);
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        app.reset(gui);
    }

    QCoreApplication::setOrganizationName(QStringLiteral("QtMeet"));
    QCoreApplication::setApplicationName(QStringLiteral("QtMeetServer"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("config"), QStringLiteral("Path to server ini file."), QStringLiteral("file")});
    parser.addOption({QStringLiteral("headless"), QStringLiteral("Run without the admin window.")});
    parser.addOption({QStringLiteral("dev"), QStringLiteral("Development mode: allow multiple server instances.")});
    parser.addOption({QStringLiteral("production"),
                      QStringLiteral("Production mode: single-instance (second launch raises the first).")});
    parser.process(*app);

    const bool development = qm::isDevelopmentMode(parser.isSet(QStringLiteral("dev")),
                                                   parser.isSet(QStringLiteral("production")));

    qm::Logger::install();
    qInfo("QtMeetServer mode: %s", development ? "development (multi-instance)" : "production (single-instance)");

    qm::AppConfig config;
    const QString configPath = parser.isSet(QStringLiteral("config"))
                                   ? parser.value(QStringLiteral("config"))
                                   : qm::AppConfig::defaultPath(QStringLiteral("server"));
    config.load(configPath);

    std::unique_ptr<qm::SingleInstance> instance;
    if (!headless && !development) {
        instance = std::make_unique<qm::SingleInstance>(QStringLiteral("QtMeet.Server.SingleInstance"));
        if (!instance->tryBecomePrimary()) {
            qm::SingleInstance::notifyPrimary(QStringLiteral("QtMeet.Server.SingleInstance"));
            return 0;
        }
    }

    qm::ServerApp server(config.server());

    if (headless) {
        if (!server.start())
            return 1;
        return app->exec();
    }

    qm::AdminController admin(&server, configPath);
    qmlRegisterSingletonInstance("QtMeet.Server", 1, 0, "Admin", &admin);

    qm::DesktopShell shell(QStringLiteral("QtMeet Server"));
    shell.setShowHotkey(config.server().showHotkey);
    qmlRegisterSingletonInstance("QtMeet.Server", 1, 0, "Shell", &shell);

    if (instance) {
        QObject::connect(instance.get(), &qm::SingleInstance::activationRequested, &shell,
                         &qm::DesktopShell::showWindow);
    }
    QObject::connect(&shell, &qm::DesktopShell::showHotkeyChanged, &shell, [&] {
        qm::AppConfig cfg;
        cfg.load(configPath);
        qm::ServerSettings s = server.settings();
        s.showHotkey = shell.showHotkey();
        const qm::ServerSettings previous = cfg.server();
        if (s.certFile.isEmpty())
            s.certFile = previous.certFile;
        if (s.keyFile.isEmpty())
            s.keyFile = previous.keyFile;
        if (s.dataDir.isEmpty())
            s.dataDir = previous.dataDir;
        s.hostedFolder = server.hostedFolder();
        cfg.setServer(s);
        cfg.save(configPath);
    });

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     app.get(), [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/AdminMain.qml")));

    if (!engine.rootObjects().isEmpty())
        shell.attachWindow(engine.rootObjects().constFirst());

    return app->exec();
}
