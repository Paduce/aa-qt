import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtMultimedia 5.15

Window {
    width: 800
    height: 480
    visible: true
    title: qsTr("Android Auto Integration")
    
    Rectangle {
        id: waitingScreen
        anchors.fill: parent
        color: "#222222"
        visible: !androidAuto.connected
        
        Column {
            anchors.centerIn: parent
            spacing: 20
            
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Waiting for Android Auto Connection...")
                color: "white"
                font.pixelSize: 24
            }
            
            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                source: "qrc:/assets/android_auto_logo.png"
                width: 200
                height: 200
                fillMode: Image.PreserveAspectFit
            }
            
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: usbDetector.isDetecting ? qsTr("USB Detection Active") : qsTr("USB Detection Inactive")
                color: usbDetector.isDetecting ? "#4CAF50" : "#F44336"
                font.pixelSize: 16
            }
        }
    }
    
    VideoOutput {
        id: androidAutoOutput
        anchors.fill: parent
        source: androidAuto
        visible: androidAuto.connected
    }
    
    Connections {
        target: usbDetector
        function onDeviceConnected(deviceId) {
            console.log("Device connected: " + deviceId);
        }
        
        function onDeviceDisconnected(deviceId) {
            console.log("Device disconnected: " + deviceId);
        }
    }
    
    Connections {
        target: androidAuto
        function onError(message) {
            console.error("Android Auto error: " + message);
        }
    }
}
