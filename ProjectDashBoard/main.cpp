#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "Carbackend.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    Carbackend carbackend;

    engine.rootContext()->setContextProperty("carbackend", &carbackend);

    const QUrl url(QStringLiteral("qrc:/Main.qml"));

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("ProjectDashBoard", "Main");

    return QGuiApplication::exec();
}
