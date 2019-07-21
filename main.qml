import QtQuick 2.9
import QtQuick.Controls 2.12
import QtQuick.Controls.Material 2.12
import QtWebSockets 1.1
import QtWebView 1.1

import Qt.labs.settings 1.0
import Qt.labs.folderlistmodel 2.12

import qmlplayground 1.0

ApplicationWindow {
    id: window
    visible: true
    width: 640
    height: 480
    title: qsTr("QmlPlaygroundClient")

    // ---------------------------------------------------------------------------------
    // Data
    // ---------------------------------------------------------------------------------

    Settings {
        id: settings

        property alias hostNameAddress: hostAddressTextField.text
        property alias toolbarmode: toolbar.manualMode
    }

    //    FolderListModel {
    //        id: projectListModel

    //        caseSensitive: false
    //        showDirs: true
    //        showDotAndDotDot: false
    //        showFiles: false

    //        rootFolder: "file:///" + appControl.projectsPath
    //        folder: "file:///" + appControl.projectsPath
    //    }

    //    FsModel {
    //        id: projectListModel
    ////        path: "file:///" + appControl.projectsPath
    //        path: appControl.projectsPath
    //    }

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

    Drawer {
        id: drawer
        width: 0.66 * window.width
        height: window.height

        Component.onCompleted: open()

        //        Loader {
        //            sourceComponent: treeDelegate
        //        }
        ListView {
            anchors.fill: parent
            model: fsModel
            //            delegate: ItemDelegate {
            //                text: "Hello"
            //            }
            delegate: treeDelegate
        }

        /*
        ListView {
            anchors.fill: parent
//            model: projectListModel
            model: fsModel

            delegate: Loader {
                sourceComponent: treeDelegate
            }

//            delegate: ItemDelegate {
//                width: parent.width
////                height: expanded ?

//                text: fileName

//                property bool expanded: false
//                onClicked: expanded = !expanded

//                FolderListModel {
//                    id:u

//                    caseSensitive: false
//                    showDirs: true
//                    showDotAndDotDot: false
//                    showFiles: false

//                    rootFolder: "file:///" + appControl.projectsPath
//                    folder: "file:///" + appControl.projectsPath
//                }
//            }
        }
        */
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

    // ---------------------------------------------------------------------------------
    // Private
    // ---------------------------------------------------------------------------------

    QtObject {
        id: internal
        property FsEntry rootItem: fsModel.root()
    }

    Component {
        id: treeDelegate

        ItemDelegate {
            id: itemDelegate

            width: parent.width
            height: visible ? expanded ? contentRow.height + childrenDelegate.height : contentRow.height :
            0

            property bool expanded: isValid && itemData.expanded
            property var itemData: entry
            property bool isValid: typeof(itemData) !== "undefined"
            property bool isDir: isValid && itemData.expandable
//            property bool isRoot: isValid && itemData.parent == internal.rootItem

            visible: isValid && ((!isDir) || (itemData.childrenCount() > 0))

            onClicked: {
                if (!isValid)
                    return;

                if (isDir)
                    itemData.expanded = !itemData.expanded
                else
                {
                    appControl.currentFile = "file:///" + itemData.path
                    drawer.close()
                }
            }

            onPressAndHold: {
                // Only for root folders
//                if (isRoot)
                if (isValid && itemData.parent == fsModel.root())
                {
                    deleteFolderPopup.entryToDelete = itemData.path
                    deleteFolderPopup.open()
                }
            }

            highlighted: {
                if (!isValid)
                    return false;

                var vPath = "file:///" + itemData.path

                return vPath === appControl.currentFile || vPath === appControl.currentFolder
            }

            Row {
                id: contentRow
                anchors.top: parent.top
                width: parent.width
                height: 40

                Image {
                    height: parent.height * 0.8
                    anchors.verticalCenter: parent.verticalCenter
                    fillMode: Image.PreserveAspectFit
                    source: isValid ? itemData.expandable && itemData.expanded ? "qrc:///img/folderOpen.svg" :
                                                                                 itemData.expandable ? "qrc:///img/folder.svg" :
                                                                                                       "qrc:///img/newFile.svg" : ""
                }

                Label {
                    height: parent.height
                    text: typeof(itemData) !== "undefined" ? "" + itemData.name : "no data"
                    verticalAlignment: Label.AlignVCenter
//                    font.bold: itemDelegate.isRoot
                }
            }

            ListView {
                id: childrenDelegate
                anchors.top: contentRow.bottom
                anchors.left: parent.left
                anchors.leftMargin: 10
                width: parent.width
                height: childrenRect.height

                visible: parent.expanded
                interactive: false

                model: typeof(itemData) !== "undefined" ? itemData.childrenCount() : 0

                delegate: Loader {
                    width: parent.width
                    height: item.height
//                    asynchronous: true
                    sourceComponent: treeDelegate

                    onLoaded: item.itemData = itemData.childAt(index)
                } // Loader
            } // ListView
        } // ItemDelegate
    } // Component

    Popup {
        id: deleteFolderPopup

        property string entryToDelete;

        width: window.width * 0.66
        height: window.height * 0.66
        x: window.width / 2 - this.width / 2
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        Material.theme: Material.Light
        padding: 0

        Page {
            id: page
            width: deleteFolderPopup.width
            height: deleteFolderPopup.height
            padding: 0

            Label {
                id: popupMessage
                width: parent.width
                wrapMode: Text.Wrap
                padding: 30
                anchors.centerIn: parent

                text: "Delete %1 permanently ?".arg(deleteFolderPopup.entryToDelete)
            }
            Label {
                id: errorMessage
                anchors.top: popupMessage.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                color: "red"
                font.pointSize: 8
//                text: "Could not delete"
                text: "" + appControl.deleteFileSystemEntryError

                Binding on visible { value: false; when: !deleteFolderPopup.visible }
            }
            Button {
                anchors.top: errorMessage.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Delete manually"
                visible: errorMessage.visible
                onClicked: {
                    Qt.openUrlExternally(deleteFolderPopup.entryToDelete + "/..")
                    deleteFolderPopup.close()
                }
            }

            footer: Pane {
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 20
                    Button {
                        text: "Yes"
                        onClicked: {
                            var success = appControl.deleteFileSystemEntry(deleteFolderPopup.entryToDelete)
                            errorMessage.visible = !success
                            if (!success)
                            {
                                errorMessage.visible = true
                                print("could not delete file system entry")
                            }
                            else
                                deleteFolderPopup.close()
                        }
                    }
                    Button {
                        text: "No"
                        onClicked: deleteFolderPopup.close()
                    }
                }
            }
        }
    }
}
