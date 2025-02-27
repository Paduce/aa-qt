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
    
    // Create Android Auto interface - use shared_ptr for proper shared_from_this() usage
    auto androidAuto = std::make_shared<AndroidAuto>();
    
    // Connect USB detection to Android Auto
    QObject::connect(&usbDetector, &UsbDetector::deviceConnected,
                     androidAuto.get(), &AndroidAuto::onDeviceConnected);
    QObject::connect(&usbDetector, &UsbDetector::deviceDisconnected,
                     androidAuto.get(), &AndroidAuto::onDeviceDisconnected);
    
    // Expose our C++ classes to QML
    engine.rootContext()->setContextProperty("usbDetector", &usbDetector);
    engine.rootContext()->setContextProperty("androidAuto", androidAuto.get());

    // Store the shared_ptr to keep AndroidAuto alive
    app.setProperty("androidAutoManager", QVariant::fromValue<void*>(androidAuto.get()));

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
