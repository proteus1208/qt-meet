#include <QApplication>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml>

#include <memory>

#include "ClientApp.h"
#include "common/AppConfig.h"
#include "common/BuildMode.h"
#include "common/DesktopShell.h"
#include "common/Logger.h"
#include "common/SingleInstance.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("QtMeet"));
    QApplication::setApplicationName(QStringLiteral("QtMeetClient"));
    QApplication::setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("config"), QStringLiteral("Path to client ini file."), QStringLiteral("file")});
    parser.addOption({QStringLiteral("dev"), QStringLiteral("Development mode: allow multiple client instances.")});
    parser.addOption({QStringLiteral("production"),
                      QStringLiteral("Production mode: single-instance (second launch raises the first).")});
    parser.process(app);

    const bool development = qm::isDevelopmentMode(parser.isSet(QStringLiteral("dev")),
                                                   parser.isSet(QStringLiteral("production")));

    std::unique_ptr<qm::SingleInstance> instance;
    if (!development) {
        const QString instanceKey = QStringLiteral("QtMeet.Client.SingleInstance");
        instance = std::make_unique<qm::SingleInstance>(instanceKey);
        if (!instance->tryBecomePrimary()) {
            qm::SingleInstance::notifyPrimary(instanceKey);
            return 0;
        }
    }

    qm::Logger::install();
    qInfo("QtMeetClient mode: %s", development ? "development (multi-instance)" : "production (single-instance)");

    const QString configPath = parser.isSet(QStringLiteral("config"))
                                   ? parser.value(QStringLiteral("config"))
                                   : qm::AppConfig::defaultPath(QStringLiteral("client"));
    qm::AppConfig config;
    config.load(configPath);

    qm::ClientApp client(config.client());
    client.registerQml();
    client.start();

    qm::DesktopShell shell(QStringLiteral("QtMeet"));
    shell.setShowHotkey(config.client().showHotkey);
    qmlRegisterSingletonInstance("QtMeet", 1, 0, "Shell", &shell);

    if (instance) {
        QObject::connect(instance.get(), &qm::SingleInstance::activationRequested, &shell,
                         &qm::DesktopShell::showWindow);
    }
    QObject::connect(&shell, &qm::DesktopShell::showHotkeyChanged, &app, [&] {
        qm::ClientSettings s = config.client();
        s.showHotkey = shell.showHotkey();
        config.setClient(s);
        config.save(configPath);
    });

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    if (!engine.rootObjects().isEmpty())
        shell.attachWindow(engine.rootObjects().constFirst());

    return app.exec();
}
