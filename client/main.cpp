#include <QApplication>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml>

#include "ClientApp.h"
#include "common/AppConfig.h"
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

    const QString instanceKey = QStringLiteral("QtMeet.Client.SingleInstance");
    qm::SingleInstance instance(instanceKey);
    if (!instance.tryBecomePrimary()) {
        qm::SingleInstance::notifyPrimary(instanceKey);
        return 0;
    }

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("config"), QStringLiteral("Path to client ini file."), QStringLiteral("file")});
    parser.process(app);

    qm::Logger::install();

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

    QObject::connect(&instance, &qm::SingleInstance::activationRequested, &shell,
                     &qm::DesktopShell::showWindow);
    QObject::connect(&shell, &qm::DesktopShell::showHotkeyChanged, &config, [&] {
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
