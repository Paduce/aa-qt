#include "androidauto.h"
#include <QDebug>
#include <QPainter>
#include <QDateTime>
#include <QImage>
#include <libusb.h>

// Include the actual implementations here, after the forward declarations in the header
#include <aasdk/USB/USBWrapper.hpp>
#include <aasdk/USB/AccessoryModeQueryChain.hpp>
#include <aasdk/USB/AccessoryModeQueryFactory.hpp>
#include <aasdk/USB/ConnectedAccessoriesEnumerator.hpp>
#include <aasdk/USB/USBHub.hpp>
#include <aasdk/USB/AOAPDevice.hpp>
#include <aasdk/TCP/TCPWrapper.hpp>
#include <aasdk/Transport/USBTransport.hpp>
#include <aasdk/Transport/TCPTransport.hpp>
#include <aasdk/Transport/SSLWrapper.hpp>
#include <aasdk/Messenger/Cryptor.hpp>
#include <aasdk/Messenger/MessageInStream.hpp>
#include <aasdk/Messenger/MessageOutStream.hpp>
#include <aasdk/Messenger/Messenger.hpp>
#include <aasdk/Messenger/ChannelId.hpp>
#include <aasdk/Channel/Control/ControlServiceChannel.hpp>
#include <aasdk/Channel/Control/IControlServiceChannelEventHandler.hpp>

// Proto includes - corrected paths based on CMake discovery
#include <aasdk_proto/ServiceDiscoveryRequestMessage.pb.h>
#include <aasdk_proto/ServiceDiscoveryResponseMessage.pb.h>
#include <aasdk_proto/AudioFocusRequestMessage.pb.h>
#include <aasdk_proto/AudioFocusResponseMessage.pb.h>
#include <aasdk_proto/ShutdownRequestMessage.pb.h>
#include <aasdk_proto/ShutdownResponseMessage.pb.h>
#include <aasdk_proto/NavigationFocusRequestMessage.pb.h>
#include <aasdk_proto/NavigationFocusResponseMessage.pb.h>
#include <aasdk_proto/PingRequestMessage.pb.h>
#include <aasdk_proto/PingResponseMessage.pb.h>
#include <aasdk/Error/Error.hpp>

AndroidAuto::AndroidAuto(QObject *parent)
    : QAbstractVideoSurface(parent), 
      m_connected(false),
      m_strand(m_ioService)
{
    // Set up a timer for simulation mode as fallback
    connect(&m_simulationTimer, &QTimer::timeout, this, &AndroidAuto::simulateFrame);
    
    // Start IO Service
    startIOServiceThread();
}

AndroidAuto::~AndroidAuto()
{
    shutdownAndroidAuto();
    stopIOServiceThread();
}

bool AndroidAuto::isConnected() const
{
    return m_connected;
}

QList<QVideoFrame::PixelFormat> AndroidAuto::supportedPixelFormats(QAbstractVideoBuffer::HandleType type) const
{
    if (type == QAbstractVideoBuffer::NoHandle) {
        return {QVideoFrame::Format_RGB32, QVideoFrame::Format_ARGB32, QVideoFrame::Format_ARGB32_Premultiplied};
    }
    return {};
}

bool AndroidAuto::start(const QVideoSurfaceFormat &format)
{
    if (isActive()) {
        stop();
    }
    
    if (!supportedPixelFormats().contains(format.pixelFormat())) {
        return false;
    }
    
    m_format = format;
    return QAbstractVideoSurface::start(format);
}

bool AndroidAuto::present(const QVideoFrame &frame)
{
    if (!isActive() || !frame.isValid()) {
        return false;
    }
    
    return QAbstractVideoSurface::present(frame);
}

void AndroidAuto::stop()
{
    QAbstractVideoSurface::stop();
}

void AndroidAuto::onDeviceConnected(const QString &deviceId)
{
    qDebug() << "Android Auto: Device connected:" << deviceId;
    initializeAndroidAuto(deviceId);
}

void AndroidAuto::onDeviceDisconnected(const QString &deviceId)
{
    qDebug() << "Android Auto: Device disconnected:" << deviceId;
    shutdownAndroidAuto();
}

void AndroidAuto::simulateFrame()
{
    if (!isActive()) {
        return;
    }
    
    // Create a simple frame for testing
    QImage image(800, 480, QImage::Format_RGB32);
    image.fill(Qt::black);
    
    QPainter painter(&image);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 24));
    
    // Add current time
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
    painter.drawText(image.rect(), Qt::AlignCenter, 
                    QString("Waiting for Android Auto Connection\n%1").arg(timeStr));
    
    // Create the video frame
    QVideoFrame frame(image);
    present(frame);
}

void AndroidAuto::startIOServiceThread()
{
    m_workLoopKeepAlive = std::make_shared<boost::asio::io_service::work>(m_ioService);
    m_ioServiceThread = std::thread([this]() {
        qDebug() << "Starting IO Service thread";
        m_ioService.run();
    });
}

void AndroidAuto::stopIOServiceThread()
{
    if (m_workLoopKeepAlive != nullptr) {
        m_workLoopKeepAlive.reset();
    }
    
    if (m_ioServiceThread.joinable()) {
        m_ioServiceThread.join();
    }
    
    qDebug() << "IO Service thread stopped";
}

void AndroidAuto::initializeAndroidAuto(const QString &deviceId)
{
    QMutexLocker locker(&m_mutex);
    
    // Shutdown any existing connection
    shutdownAndroidAuto();
    
    try {
        // Initialize USB components
        m_usbWrapper = std::make_shared<aasdk::usb::USBWrapper>();
        auto queryFactory = std::make_shared<aasdk::usb::AccessoryModeQueryFactory>(*m_usbWrapper);
        auto queryChainFactory = std::make_shared<aasdk::usb::AccessoryModeQueryChainFactory>(*m_usbWrapper, m_ioService, *queryFactory);
        
        // Create USB hub
        m_usbHub = std::make_shared<aasdk::usb::USBHub>(*m_usbWrapper, m_ioService, *queryChainFactory);
        
        // Initialize TCP components
        m_tcpWrapper = std::make_shared<aasdk::tcp::TCPWrapper>();
        
        // Set up USB enumeration
        auto connectedEnumerator = std::make_shared<aasdk::usb::ConnectedAccessoriesEnumerator>(*m_usbWrapper, m_ioService, *queryChainFactory);
        connectedEnumerator->enumerate([this, self = this->shared_from_this()](auto handle) mutable {
            if (handle != nullptr) {
                handleUSBDevice(std::move(handle));
            }
        });
        
        // Start USB hub to detect future devices
        m_usbHub->start([this, self = this->shared_from_this()](auto handle) mutable {
            if (handle != nullptr) {
                handleUSBDevice(std::move(handle));
            }
        });
        
        qDebug() << "Android Auto initialization complete";
        
        // Start video surface if needed
        if (!isActive()) {
            QVideoSurfaceFormat format(QSize(800, 480), QVideoFrame::Format_RGB32);
            start(format);
        }
        
        // Start simulation for now
        m_simulationTimer.start(100);
    }
    catch (const std::exception& ex) {
        qDebug() << "Error initializing Android Auto:" << ex.what();
        emit error(QString("Failed to initialize Android Auto: %1").arg(ex.what()));
        
        // Fall back to simulation mode
        m_simulationTimer.start(100);
    }
}

void AndroidAuto::handleUSBDevice(std::shared_ptr<libusb_device_handle> deviceHandle)
{
    try {
        qDebug() << "USB device connected, setting up Android Auto";
        
        auto aoapDevice = aasdk::usb::AOAPDevice::create(*m_usbWrapper, m_ioService, deviceHandle);
        m_transport = std::make_shared<aasdk::transport::USBTransport>(m_ioService, aoapDevice);
        m_transport->start();
        
        // Set up SSL and cryptography
        m_sslWrapper = std::make_shared<aasdk::transport::SSLWrapper>();
        m_cryptor = std::make_shared<aasdk::messenger::Cryptor>(m_sslWrapper);
        
        // Set up messenger
        m_messageInStream = std::make_shared<aasdk::messenger::MessageInStream>(m_ioService, m_transport, m_cryptor);
        m_messageOutStream = std::make_shared<aasdk::messenger::MessageOutStream>(m_ioService, m_transport, m_cryptor);
        m_messenger = std::make_shared<aasdk::messenger::Messenger>(m_ioService, m_messageInStream, m_messageOutStream);
        
        // Set up control channel
        m_controlServiceChannel = std::make_shared<aasdk::channel::control::ControlServiceChannel>(m_strand, m_messenger);
        m_controlServiceChannel->receive(this->shared_from_this());
        
        m_connected = true;
        emit connectedChanged();
        
        // Stop simulation
        m_simulationTimer.stop();
        
        qDebug() << "Android Auto device setup complete";
    }
    catch(const std::exception& ex) {
        qDebug() << "Exception during device setup:" << ex.what();
        emit error(QString("Error during device setup: %1").arg(ex.what()));
    }
}

void AndroidAuto::shutdownAndroidAuto()
{
    QMutexLocker locker(&m_mutex);
    
    // Stop video surface
    if (isActive()) {
        stop();
    }
    
    // Stop simulation timer
    m_simulationTimer.stop();
    
    try {
        // Stop control channel
        if(m_controlServiceChannel != nullptr) {
            m_controlServiceChannel->stop();
        }
        
        // Stop transport
        if(m_transport != nullptr) {
            m_transport->stop();
        }
        
        // Stop USB hub
        if (m_usbHub != nullptr) {
            m_usbHub->stop();
        }
        
        // Clear all shared pointers
        m_controlServiceChannel.reset();
        m_messenger.reset();
        m_messageInStream.reset();
        m_messageOutStream.reset();
        m_cryptor.reset();
        m_sslWrapper.reset();
        m_transport.reset();
        m_usbWrapper.reset();
        m_usbHub.reset();
        m_tcpWrapper.reset();
    }
    catch (const std::exception& ex) {
        qDebug() << "Error shutting down Android Auto:" << ex.what();
    }
    
    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
    }
}

// Control channel event handlers
void AndroidAuto::onServiceDiscoveryRequest(const aasdk::proto::messages::ServiceDiscoveryRequest& request)
{
    qDebug() << "Service discovery request received";
    
    aasdk::proto::messages::ServiceDiscoveryResponse response;
    response.mutable_channel_descriptors()->Add()->set_channel_id(aasdk::messenger::ChannelId::VIDEO);
    response.mutable_channel_descriptors()->Add()->set_channel_id(aasdk::messenger::ChannelId::SENSOR);
    response.mutable_channel_descriptors()->Add()->set_channel_id(aasdk::messenger::ChannelId::AUDIO_INPUT);
    response.mutable_channel_descriptors()->Add()->set_channel_id(aasdk::messenger::ChannelId::INPUT);
    response.mutable_channel_descriptors()->Add()->set_channel_id(aasdk::messenger::ChannelId::NAVIGATION);
    
    m_controlServiceChannel->sendServiceDiscoveryResponse(response);
    m_controlServiceChannel->receive(this->shared_from_this());
}

void AndroidAuto::onAudioFocusRequest(const aasdk::proto::messages::AudioFocusRequest& request)
{
    qDebug() << "Audio focus request received";
    
    aasdk::proto::messages::AudioFocusResponse response;
    response.set_audio_focus_state(aasdk::proto::enums::AudioFocusState::FOCUSED);
    
    m_controlServiceChannel->sendAudioFocusResponse(response);
    m_controlServiceChannel->receive(this->shared_from_this());
}

void AndroidAuto::onShutdownRequest(const aasdk::proto::messages::ShutdownRequest& request)
{
    qDebug() << "Shutdown request received";
    
    aasdk::proto::messages::ShutdownResponse response;
    m_controlServiceChannel->sendShutdownResponse(response);
    
    shutdownAndroidAuto();
}

void AndroidAuto::onShutdownResponse(const aasdk::proto::messages::ShutdownResponse& response)
{
    qDebug() << "Shutdown response received";
    
    shutdownAndroidAuto();
}

void AndroidAuto::onNavigationFocusRequest(const aasdk::proto::messages::NavigationFocusRequest& request)
{
    qDebug() << "Navigation focus request received";
    
    aasdk::proto::messages::NavigationFocusResponse response;
    response.set_type(aasdk::proto::enums::NavigationFocusType::FOCUSED);
    
    m_controlServiceChannel->sendNavigationFocusResponse(response);
    m_controlServiceChannel->receive(this->shared_from_this());
}

void AndroidAuto::onNavigationFocusResponse(const aasdk::proto::messages::NavigationFocusResponse& response)
{
    qDebug() << "Navigation focus response received";
    
    m_controlServiceChannel->receive(this->shared_from_this());
}

void AndroidAuto::onPingRequest(const aasdk::proto::messages::PingRequest& request)
{
    qDebug() << "Ping request received";
    
    aasdk::proto::messages::PingResponse response;
    m_controlServiceChannel->sendPingResponse(response);
    m_controlServiceChannel->receive(this->shared_from_this());
}

void AndroidAuto::onPingResponse(const aasdk::proto::messages::PingResponse& response)
{
    qDebug() << "Ping response received";
    
    m_controlServiceChannel->receive(this->shared_from_this());
}

void AndroidAuto::onChannelError(const aasdk::error::Error& e)
{
    qDebug() << "Channel error:" << e.what();
    
    shutdownAndroidAuto();
    
    // Fall back to simulation mode
    m_simulationTimer.start(100);
}
