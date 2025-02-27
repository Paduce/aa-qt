#ifndef ANDROIDAUTO_H
#define ANDROIDAUTO_H

#include <QObject>
#include <QAbstractVideoSurface>
#include <QVideoSurfaceFormat>
#include <QMutex>
#include <QTimer>
#include <memory>
#include <boost/asio.hpp>
#include <thread>

// Forward declaration for libusb
struct libusb_device_handle;

// Forward declarations for aasdk types to avoid include errors
namespace aasdk {
    namespace usb {
        class USBWrapper;
        class AccessoryModeQueryChainFactory;  // Added factory
        class USBHub;
    }
    namespace tcp {
        class TCPWrapper;
    }
    namespace transport {
        class Transport;  // Use generic base class
        class USBTransport;
        class SSLWrapper;
    }
    namespace messenger {
        class Cryptor;
        class MessageInStream;
        class MessageOutStream;
        class Messenger;
    }
    namespace channel {
        namespace control {
            class ControlServiceChannel;
            class IControlServiceChannelEventHandler;  // Include the actual interface
        }
    }
    namespace proto {
        namespace messages {
            class ServiceDiscoveryRequest;
            class ServiceDiscoveryResponse;  // Added response
            class AudioFocusRequest;
            class AudioFocusResponse;  // Added response
            class ShutdownRequest;
            class ShutdownResponse;
            class NavigationFocusRequest;
            class NavigationFocusResponse;
            class PingRequest;
            class PingResponse;
        }
    }
    namespace error {
        class Error;
    }
}

class AndroidAuto : public QAbstractVideoSurface, 
                    public aasdk::channel::control::IControlServiceChannelEventHandler,  // Use the actual interface
                    public std::enable_shared_from_this<AndroidAuto>
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    
public:
    explicit AndroidAuto(QObject *parent = nullptr);
    ~AndroidAuto() override;
    
    bool isConnected() const;
    
    // QAbstractVideoSurface interface
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType type = QAbstractVideoBuffer::NoHandle) const override;
    bool start(const QVideoSurfaceFormat &format) override;
    bool present(const QVideoFrame &frame) override;
    void stop() override;
    
    // Control channel event handlers
    void onServiceDiscoveryRequest(const aasdk::proto::messages::ServiceDiscoveryRequest& request) override;
    void onAudioFocusRequest(const aasdk::proto::messages::AudioFocusRequest& request) override;
    void onShutdownRequest(const aasdk::proto::messages::ShutdownRequest& request) override;
    void onShutdownResponse(const aasdk::proto::messages::ShutdownResponse& response) override;
    void onNavigationFocusRequest(const aasdk::proto::messages::NavigationFocusRequest& request) override;
    void onNavigationFocusResponse(const aasdk::proto::messages::NavigationFocusResponse& response) override;
    void onPingRequest(const aasdk::proto::messages::PingRequest& request) override;
    void onPingResponse(const aasdk::proto::messages::PingResponse& response) override;
    void onChannelError(const aasdk::error::Error& e) override;
    
public slots:
    void onDeviceConnected(const QString &deviceId);
    void onDeviceDisconnected(const QString &deviceId);
    
signals:
    void connectedChanged();
    void error(const QString &message);
    
private:
    bool m_connected;
    QVideoSurfaceFormat m_format;
    QMutex m_mutex;
    QTimer m_simulationTimer;
    
    // aasdk components
    boost::asio::io_service m_ioService;
    std::shared_ptr<boost::asio::io_service::work> m_workLoopKeepAlive;
    boost::asio::io_service::strand m_strand;
    std::shared_ptr<aasdk::usb::USBWrapper> m_usbWrapper;
    std::shared_ptr<aasdk::usb::USBHub> m_usbHub;
    std::shared_ptr<aasdk::tcp::TCPWrapper> m_tcpWrapper;
    std::shared_ptr<aasdk::transport::Transport> m_transport;  // Use base Transport class
    std::shared_ptr<aasdk::transport::SSLWrapper> m_sslWrapper;
    std::shared_ptr<aasdk::messenger::Cryptor> m_cryptor;
    std::shared_ptr<aasdk::messenger::MessageInStream> m_messageInStream;
    std::shared_ptr<aasdk::messenger::MessageOutStream> m_messageOutStream;
    std::shared_ptr<aasdk::messenger::Messenger> m_messenger;
    std::shared_ptr<aasdk::channel::control::ControlServiceChannel> m_controlServiceChannel;
    
    std::thread m_ioServiceThread;
    
    void initializeAndroidAuto(const QString &deviceId);
    void shutdownAndroidAuto();
    void startIOServiceThread();
    void stopIOServiceThread();
    void handleUSBDevice(std::shared_ptr<libusb_device_handle> deviceHandle);

private slots:
    void simulateFrame();
};

#endif // ANDROIDAUTO_H
