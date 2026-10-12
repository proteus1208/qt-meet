#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include "ClientApp.h"
#include "common/AppConfig.h"
#include "common/Logger.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("QtMeet"));
    QGuiApplication::setApplicationName(QStringLiteral("QtMeetClient"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("config"), QStringLiteral("Path to client ini file."), QStringLiteral("file")});
    parser.process(app);

    qm::Logger::install();

    qm::AppConfig config;
    config.load(parser.isSet(QStringLiteral("config"))
                    ? parser.value(QStringLiteral("config"))
                    : qm::AppConfig::defaultPath(QStringLiteral("client")));

    qm::ClientApp client(config.client());
    client.registerQml();
    client.start();

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    return app.exec();
}
