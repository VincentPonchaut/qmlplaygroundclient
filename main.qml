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
    // Data
    // ---------------------------------------------------------------------------------

//    WebSocket {
//        id: socket
//        url: "ws://%1".arg(appControl.activeServerIp)
//        active: true

//        onTextMessageReceived: {
//            //print("MESSAGE RECEIVED \n" + message);

//            if (appControl.messageContent(String(message), "messagetype") == "data")
//            {
//                print("MESSAGE RECEIVED \n" + message);
//                parseData(appControl.messageContent(message, "json"));
//            }
//            else
//                appControl.onTextMessageReceived(message);
//        }

//        onBinaryMessageReceived: {
//            print("Binary message received");
//            appControl.onBinaryMessageReceived(message);
//        }
//    }

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

//            model: appControl.availableServers
//            model: appControl.availableAddresses
            model: appControl.hosts
            textRole: "id"

            font.pointSize: 12

            delegate: ItemDelegate {
                id: serverChoiceDelegate
                width: serverCombobox.width

                property bool isCurrent: index == serverCombobox.currentIndex

                contentItem: Column {
                    spacing: 5
                    Label {
                        id: hostIdLabel
                        text: serverChoiceDelegate.isCurrent ? "[Current] " + modelData.id : modelData.id
                        font.family: serverCombobox.font.family
                        font.pointSize: serverCombobox.font.pointSize
                        font.bold: serverChoiceDelegate.isCurrent
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                    Label {
                        text: modelData.address
//                        font.italic: true
                        font.pointSize: hostIdLabel.font.pointSize - 2
                        color: index != serverCombobox.currentIndex ? Qt.darker(hostIdLabel.color, 1.35) :
                                                                      hostIdLabel.color
                    }
                }
                highlighted: serverCombobox.highlightedIndex === index
            }
            /*
            contentItem: Text {
                leftPadding: 0
                rightPadding: serverCombobox.indicator.width + serverCombobox.spacing

                text: "zozo:" + modelData
                font: serverCombobox.font
                color: serverCombobox.pressed ? "#17a81a" : "#21be2b"
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }*/

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
            text: !toolbar.manualMode ? "<b><u>Auto</u></b>/Manual" :
                                        "Auto/<b><u>Manual</u></b>"
            onClicked: {
                toolbar.manualMode = !toolbar.manualMode
                refreshActiveIp()
            }
        }
    }

    Rectangle {
        id: background
        anchors.fill: parent
        color: "white"
    }

    Loader {
        id: contentLoader
        anchors.fill: parent
//        source: appControl.currentFile.length == 0 ? "" :
//                appControl.currentFile + "?=" + Date.now();
//        asynchronous: true
    }

    Rectangle {
        anchors.fill: parent
        color: "white"
        visible: contentLoader.status === Loader.Error

        Label {
            anchors.centerIn: parent
            width: parent.width * 0.88
            verticalAlignment: Text.AlignVCenter
            color: "black"

            text: "An error occurred while loading the file.\n%1\n%2"
                    .arg(contentLoader.source)
                    .arg(contentLoader.sourceComponent != null ? contentLoader.sourceComponent.errorString() : "")
            wrapMode: Label.Wrap
        }
    }

    Rectangle {
        id: loadingOverlay
        anchors.fill: parent
        visible: appControl.isProcessing || loadingTimer.running

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

            //text: appControl.status
            text: "Loading..."
            font.pointSize: 14
            font.family: "Segoe UI Light"
        }
    }

    // ---------------------------------------------------------------------------------
    // Logic
    // ---------------------------------------------------------------------------------

//    Timer {
//        interval: 16
//        repeat: true
//        running: contentLoader.status === Loader.Error
//        onTriggered: {
//            var vCurrentFile = appControl.currentFile
//            appControl.currentFile = ""
//            appControl.currentFile = vCurrentFile
//        }
//    }

    Component.onCompleted: {
        refreshActiveIp()
    }
    function refreshActiveIp() {
        if (toolbar.manualMode)
        {
            appControl.setActiveServerIp(hostAddressTextField.text)
        }
        else
        {
            var selectedHost = appControl.hosts[serverCombobox.currentIndex]
            if (typeof(selectedHost) == "undefined")
                return;

            var wasAlreadySet = (appControl.activeServerIp == selectedHost.address)
            appControl.setActiveServerIp(selectedHost.address)

            // emit manually to refresh the socket
            if (wasAlreadySet)
                appControl.activeServerIpChanged(selectedHost.address)
//            appControl.setActiveServerIp(serverCombobox.currentText)
//            appControl.selectAvailableHost(serverCombobox.currentIndex)
//            appControl.setActiveServerIp(appControl.availableHosts[serverCombobox.currentIndex])
        }
    }

    property var lastObject: null
    Connections {
        target: appControl;
        onIsProcessingChanged: {
            if (appControl.isProcessing)
                loadingTimer.start()
//            else
//                contentLoader.source = appControl.currentFile + "?=" + Date.now()
        }
        onCurrentFileChanged: {
            if (appControl.currentFile.length > 0)
                contentLoader.source = appControl.currentFile + "?=" + Date.now()
//            print("hella")
//            renderQml(appControl.readFileContents(appControl.currentFile));
//            print("hello")
        }
        onJsonMessage: {
//            print("heyo", message)
            parseData(message);
        }
    }

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
