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

// Include the actual header for IControlServiceChannelEventHandler
#include <aasdk/Channel/Control/IControlServiceChannelEventHandler.hpp>

// Forward declaration for libusb
struct libusb_device_handle;

namespace aasdk {
    namespace usb {
        class IUSBWrapper;
        class USBWrapper;
        class IAccessoryModeQueryFactory;
        class AccessoryModeQueryFactory;
        class IAccessoryModeQueryChainFactory;
        class AccessoryModeQueryChainFactory;
        class IUSBHub;
        class USBHub;
        class IConnectedAccessoriesEnumerator;
        class ConnectedAccessoriesEnumerator;
    }
    namespace tcp {
        class ITCPWrapper;
        class TCPWrapper;
    }
    namespace transport {
        class ITransport;
        class USBTransport;
        class ISSLWrapper;
        class SSLWrapper;
    }
    namespace messenger {
        class ICryptor;
        class Cryptor;
        class IMessageInStream;
        class MessageInStream;
        class IMessageOutStream;
        class MessageOutStream;
        class IMessenger;
        class Messenger;
    }
    namespace io {
        template<typename ReplyType>
        class IPromise;
        
        template<typename ReplyType>
        using Promise = IPromise<ReplyType>;
        
        template<typename ReplyType>
        using PromisePtr = std::shared_ptr<Promise<ReplyType>>;
    }
}

class AndroidAuto : public QAbstractVideoSurface, 
                    public aasdk::channel::control::IControlServiceChannelEventHandler,
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
    void onServiceDiscoveryRequest(const aasdk::proto::messages::ServiceDiscoveryRequest& request,
                                 aasdk::messenger::Timestamp::value_type timestamp) override;
    void onAudioFocusRequest(const aasdk::proto::messages::AudioFocusRequest& request,
                           aasdk::messenger::Timestamp::value_type timestamp) override;
    void onShutdownRequest(const aasdk::proto::messages::ShutdownRequest& request,
                         aasdk::messenger::Timestamp::value_type timestamp) override;
    void onShutdownResponse(const aasdk::proto::messages::ShutdownResponse& response,
                          aasdk::messenger::Timestamp::value_type timestamp) override;
    void onNavigationFocusRequest(const aasdk::proto::messages::NavigationFocusRequest& request,
                                aasdk::messenger::Timestamp::value_type timestamp) override;
    void onNavigationFocusResponse(const aasdk::proto::messages::NavigationFocusResponse& response,
                                 aasdk::messenger::Timestamp::value_type timestamp) override;
    void onPingRequest(const aasdk::proto::messages::PingRequest& request,
                     aasdk::messenger::Timestamp::value_type timestamp) override;
    void onPingResponse(const aasdk::proto::messages::PingResponse& response,
                      aasdk::messenger::Timestamp::value_type timestamp) override;
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
    std::shared_ptr<aasdk::usb::IUSBWrapper> m_usbWrapper;
    std::shared_ptr<aasdk::usb::IUSBHub> m_usbHub;
    std::shared_ptr<aasdk::tcp::ITCPWrapper> m_tcpWrapper;
    std::shared_ptr<aasdk::transport::ITransport> m_transport;
    std::shared_ptr<aasdk::transport::ISSLWrapper> m_sslWrapper;
    std::shared_ptr<aasdk::messenger::ICryptor> m_cryptor;
    std::shared_ptr<aasdk::messenger::IMessageInStream> m_messageInStream;
    std::shared_ptr<aasdk::messenger::IMessageOutStream> m_messageOutStream;
    std::shared_ptr<aasdk::messenger::IMessenger> m_messenger;
    std::shared_ptr<aasdk::channel::control::IControlServiceChannel> m_controlServiceChannel;
    
    std::thread m_ioServiceThread;
    
    void initializeAndroidAuto(const QString &deviceId);
    void shutdownAndroidAuto();
    void startIOServiceThread();
    void stopIOServiceThread();
    void handleUSBDevice(std::shared_ptr<libusb_device_handle> deviceHandle);
    
    // Promise handlers
    void onEnumerateResult(std::shared_ptr<libusb_device_handle> handle);
    void onUSBHubResult(std::shared_ptr<libusb_device_handle> handle);

private slots:
    void simulateFrame();
};

#endif // ANDROIDAUTO_H
