#include "usbdetector.h"
#include <QDebug>

UsbDetectionThread::UsbDetectionThread(QObject *parent)
    : QThread(parent), m_running(false), m_usbContext(nullptr)
{
}

UsbDetectionThread::~UsbDetectionThread()
{
    stop();
    wait();
    if (m_usbContext) {
        libusb_exit(m_usbContext);
    }
}

void UsbDetectionThread::stop()
{
    m_running = false;
}

void UsbDetectionThread::run()
{
    m_running = true;
    
    if (libusb_init(&m_usbContext) < 0) {
        emit error("Failed to initialize libusb");
        return;
    }
    
    // Set up hot plug callback if supported
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        qWarning() << "Hotplug capabilities not supported";
        // Fall back to polling
        libusb_device **devs;
        int currentDeviceCount = 0;
        
        while (m_running) {
            ssize_t cnt = libusb_get_device_list(m_usbContext, &devs);
            if (cnt < 0) {
                emit error("Failed to get device list");
                break;
            }
            
            // Check for new devices
            if (cnt != currentDeviceCount) {
                qDebug() << "Device count changed from" << currentDeviceCount << "to" << cnt;
                currentDeviceCount = cnt;
                
                // Just emit a generic device connected signal
                // In a real implementation, we would check for specific Android devices
                emit deviceConnected("generic");
            }
            
            libusb_free_device_list(devs, 1);
            QThread::msleep(1000); // Poll every second
        }
    } else {
        // Hot plug is supported, use the callback system
        libusb_hotplug_callback_handle callbackHandle;
        
        // Android Auto VID/PID for detection (general Android values)
        // Google's Vendor ID for reference: 0x18d1
        int vendorId = LIBUSB_HOTPLUG_MATCH_ANY;  // Any vendor ID
        int productId = LIBUSB_HOTPLUG_MATCH_ANY; // Any product ID
        
        int rc = libusb_hotplug_register_callback(
            m_usbContext,
            static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
            LIBUSB_HOTPLUG_ENUMERATE,
            vendorId,
            productId,
            LIBUSB_HOTPLUG_MATCH_ANY, // Match any device class
            [](libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *userData) -> int {
                UsbDetectionThread *self = static_cast<UsbDetectionThread*>(userData);
                
                struct libusb_device_descriptor desc;
                libusb_get_device_descriptor(device, &desc);
                
                QString deviceId = QString("%1:%2").arg(desc.idVendor, 4, 16, QChar('0')).arg(desc.idProduct, 4, 16, QChar('0'));
                
                if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
                    emit self->deviceConnected(deviceId);
                } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
                    emit self->deviceDisconnected(deviceId);
                }
                
                return 0;
            },
            this,
            &callbackHandle
        );
        
        if (rc != LIBUSB_SUCCESS) {
            emit error("Failed to register hotplug callback");
            return;
        }
        
        // Process USB events
        while (m_running) {
            libusb_handle_events_completed(m_usbContext, nullptr);
            QThread::msleep(100); // Sleep to prevent high CPU usage
        }
        
        // Deregister callback
        libusb_hotplug_deregister_callback(m_usbContext, callbackHandle);
    }
}

UsbDetector::UsbDetector(QObject *parent)
    : QObject(parent), m_detectionThread(nullptr), m_isDetecting(false)
{
}

UsbDetector::~UsbDetector()
{
    stopDetection();
}

bool UsbDetector::isDetecting() const
{
    return m_isDetecting;
}

void UsbDetector::startDetection()
{
    if (m_isDetecting) {
        return;
    }
    
    m_detectionThread = new UsbDetectionThread(this);
    connect(m_detectionThread, &UsbDetectionThread::deviceConnected,
            this, &UsbDetector::deviceConnected);
    connect(m_detectionThread, &UsbDetectionThread::deviceDisconnected,
            this, &UsbDetector::deviceDisconnected);
    connect(m_detectionThread, &UsbDetectionThread::error,
            this, &UsbDetector::onThreadError);
    
    m_detectionThread->start();
    m_isDetecting = true;
    emit isDetectingChanged();
}

void UsbDetector::stopDetection()
{
    if (!m_isDetecting) {
        return;
    }
    
    if (m_detectionThread) {
        m_detectionThread->stop();
        m_detectionThread->wait();
        delete m_detectionThread;
        m_detectionThread = nullptr;
    }
    
    m_isDetecting = false;
    emit isDetectingChanged();
}

void UsbDetector::onThreadError(const QString &message)
{
    qWarning() << "USB Detection error:" << message;
    emit error(message);
    stopDetection();
}
