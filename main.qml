import QtQuick 2.9
import QtQuick.Controls 2.2
import QtWebSockets 1.1
import Qt.labs.settings 1.0

ApplicationWindow {
    id: window
    visible: true
    width: 640
    height: 480
    title: qsTr("Stack")

    // ---------------------------------------------------------------------------------
    // View
    // ---------------------------------------------------------------------------------

    header: ToolBar {
        contentHeight: toolButton.implicitHeight

        TextField {
            id: hostAddressTextField

            width: parent.width * 0.88
            anchors.centerIn: parent

            placeholderText: "Enter host ip address (you must be on the same network)"
            onAccepted: focus = false
        }
    }

    Pane {
        id: contentPane
        anchors.fill: parent
    }

    // ---------------------------------------------------------------------------------
    // Data
    // ---------------------------------------------------------------------------------

    WebSocket {
        id: socket
        url: "ws://%1".arg(hostAddressTextField.text)
        active: true

        onTextMessageReceived: renderQml(message);
    }

    Settings {
        id: settings

        property alias hostNameAddress: hostAddressTextField.text
    }

    // ---------------------------------------------------------------------------------
    // Logic
    // ---------------------------------------------------------------------------------

    property var lastObject: null

    function renderQml(pQmlStr)
    {
        if (lastObject !== null)
            lastObject.destroy()

        lastObject = Qt.createQmlObject(pQmlStr,
                                        contentPane,
                                        "content_object");
    }
}