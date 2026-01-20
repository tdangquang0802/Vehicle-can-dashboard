#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "realtime.h"
#include "clustercontroller.h"
#include "ecucommandclient.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<RealTime>("RealTimeLib", 1, 0, "RealTime");

    ClusterController cluster;
    EcuCommandClient ecuCmd;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("cluster", &cluster);
    engine.rootContext()->setContextProperty("ecuCmd", &ecuCmd);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("CAN_Dashboard", "Main");
    return app.exec();
}
