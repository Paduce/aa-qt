#ifndef USBDETECTOR_H
#define USBDETECTOR_H

#include <QObject>
#include <QThread>
#include <QString>

// Use a more system-independent include
#include <libusb.h>

class UsbDetectionThread : public QThread
{
    Q_OBJECT
public:
    explicit UsbDetectionThread(QObject *parent = nullptr);
    ~UsbDetectionThread() override;
    
    void stop();

protected:
    void run() override;

signals:
    void deviceConnected(const QString &deviceId);
    void deviceDisconnected(const QString &deviceId);
    void error(const QString &message);

private:
    bool m_running;
    libusb_context *m_usbContext;
};

class UsbDetector : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isDetecting READ isDetecting NOTIFY isDetectingChanged)
    
public:
    explicit UsbDetector(QObject *parent = nullptr);
    ~UsbDetector() override;
    
    bool isDetecting() const;
    Q_INVOKABLE void startDetection();
    Q_INVOKABLE void stopDetection();
    
signals:
    void deviceConnected(const QString &deviceId);
    void deviceDisconnected(const QString &deviceId);
    void isDetectingChanged();
    void error(const QString &message);
    
private slots:
    void onThreadError(const QString &message);
    
private:
    UsbDetectionThread *m_detectionThread;
    bool m_isDetecting;
};

#endif // USBDETECTOR_H
