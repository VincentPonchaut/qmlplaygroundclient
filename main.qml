import QtQuick 2.9
import QtQuick.Controls 2.2
import QtWebSockets 1.1
import Qt.labs.settings 1.0
import QtWebView 1.1

ApplicationWindow {
    id: window
    visible: true
    width: 640
    height: 480
    title: qsTr("QmlPlaygroundClient")

    // ---------------------------------------------------------------------------------
    // View
    // ---------------------------------------------------------------------------------

    header: ToolBar {
        id: toolbar
        width: parent.width // explicit it here to avoid hostAddressTextField not being sized properly
        contentHeight: hostAddressTextField.implicitHeight

        property bool manualMode: false

        padding: 10

        ComboBox {
            id: serverCombobox
            width: (parent.width - switchModeButton.width) - 10
            anchors.verticalCenter: parent.verticalCenter
            visible: !toolbar.manualMode

            model: appControl.availableServers
            onModelChanged: refreshActiveIp()
            onCurrentIndexChanged: refreshActiveIp()
        }

        TextField {
            id: hostAddressTextField
            width: (parent.width - switchModeButton.width) - 10
//                anchors.verticalCenter: switchModeButton.verticalCenter
//            anchors.baseline: switchModeButton.baseline
//            anchors.baselineOffset: switchModeButton.baselineOffset
            height: switchModeButton.height
            anchors.bottom: switchModeButton.bottom
            visible: toolbar.manualMode

            placeholderText: "Enter host ip address (you must be on the same network)"
            onAccepted: focus = false
            onTextChanged: {
                if (toolbar.manualMode)
                    appControl.setActiveServerIp(hostAddressTextField.text)
            }
        }

        Button {
            id: switchModeButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            checkable: true
            text: !toolbar.manualMode ? "<b><u>Select</u></b>/Manual" :
                                        "Select/<b><u>Manual</u></b>"
            onClicked: {
                toolbar.manualMode = !toolbar.manualMode
                refreshActiveIp()
            }
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
        url: "ws://%1".arg(appControl.activeServerIp)
        active: true

        onTextMessageReceived: {
            //print("MESSAGE RECEIVED \n" + message);

            if (appControl.messageContent(String(message), "messagetype") == "data")
            {
                print("MESSAGE RECEIVED \n" + message);
                parseData(appControl.messageContent(message, "json"));
            }
            else
                appControl.onTextMessageReceived(message);
        }

        onBinaryMessageReceived: {
            print("Binary message received");
            appControl.onBinaryMessageReceived(message);
        }
    }

    Settings {
        id: settings

        property alias hostNameAddress: hostAddressTextField.text
        property alias toolbarmode: toolbar.manualMode
//        property alias avalaibleServers: appControl.availableServers
//        property alias comboboxCurrentIndex: serverCombobox.currentIndex
    }

    // ---------------------------------------------------------------------------------
    // View
    // ---------------------------------------------------------------------------------

    Loader {
        anchors.fill: parent
        source: appControl.currentFile
    }

    Rectangle {
        id: loadingOverlay
        anchors.fill: parent
        visible: loadingTimer.running

        color: Qt.rgba(0,0,0, 0.6)
        z: 100

        Timer {
            id: loadingTimer
            interval: 1000 // minimum loading time
            repeat: false;
            // If after timeout, processing is still ongoing, restart
            onTriggered: if (appControl.isProcessing) loadingTimer.start()
        }

        BusyIndicator {
            id: loadingIndicator
            anchors.centerIn: parent
            running: visible
        }
        Label {
            anchors.top: loadingIndicator.bottom
            anchors.horizontalCenter: loadingIndicator.horizontalCenter

            text: appControl.status
            font.pointSize: 14
            font.family: "Segoe UI Light"
        }
    }

    // ---------------------------------------------------------------------------------
    // Logic
    // ---------------------------------------------------------------------------------

    Component.onCompleted: {
        refreshActiveIp()
    }
    function refreshActiveIp() {
        if (toolbar.manualMode)
            appControl.setActiveServerIp(hostAddressTextField.text)
        else
            appControl.setActiveServerIp(serverCombobox.currentText)
    }

    property var lastObject: null
    Connections {
        target: appControl;
        onIsProcessingChanged: if (appControl.isProcessing) loadingTimer.start()
    }
//    Connections {
//        target: appControl
//        onCurrentFileChanged: {
//            print("hella")
//            renderQml(appControl.readFileContents(appControl.currentFile));
//            print("hello")
//        }
//    }

    function renderQml(pQmlStr)
    {
        if (lastObject !== null)
            lastObject.destroy()

        lastObject = Qt.createQmlObject(pQmlStr,
                                        contentPane,
                                        "content_object");
    }

    function parseData(vDataObject)
    {
        // Parse the JSON
        vDataObject = JSON.parse(vDataObject);

        // Iterate and require property adding
        for (var vProperty in vDataObject) if (vDataObject.hasOwnProperty(vProperty))
        {
            appControl.addContextProperty(vProperty, vDataObject[vProperty]);
        }
    }
}
