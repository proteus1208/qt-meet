import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMeet.Server

ApplicationWindow {
    id: window
    width: 640
    height: 480
    visible: true
    title: qsTr("QtMeet Server")

    FolderDialog {
        id: folderDialog
        title: qsTr("Select folder to host")
        onAccepted: Admin.chooseFolder(selectedFolder)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Hosted folder:") }
            TextField {
                Layout.fillWidth: true
                readOnly: true
                text: Admin.hostedFolder
                placeholderText: qsTr("No folder selected")
            }
            Button {
                text: qsTr("Browse...")
                onClicked: folderDialog.open()
            }
        }

        RowLayout {
            Button {
                text: Admin.running ? qsTr("Stop") : qsTr("Start")
                onClicked: Admin.running ? Admin.stop() : Admin.start()
            }
            Label { text: Admin.listenInfo }
            Item { Layout.fillWidth: true }
            Label { text: qsTr("Connections: %1").arg(Admin.connectionCount) }
        }

        Label { text: qsTr("Online users") }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Admin.onlineUsers
            delegate: ItemDelegate {
                required property string modelData
                width: ListView.view.width
                text: modelData
            }
        }
    }
}
