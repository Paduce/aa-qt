#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "src/usbdetector.h"
#include "src/androidauto.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    // Create USB detector
    UsbDetector usbDetector;
    
    // Create Android Auto interface
    AndroidAuto androidAuto;
    
    // Connect USB detection to Android Auto
    QObject::connect(&usbDetector, &UsbDetector::deviceConnected,
                     &androidAuto, &AndroidAuto::onDeviceConnected);
    QObject::connect(&usbDetector, &UsbDetector::deviceDisconnected,
                     &androidAuto, &AndroidAuto::onDeviceDisconnected);
    
    // Expose our C++ classes to QML
    engine.rootContext()->setContextProperty("usbDetector", &usbDetector);
    engine.rootContext()->setContextProperty("androidAuto", &androidAuto);

    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    // Start USB detection
    usbDetector.startDetection();

    return app.exec();
}
