import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMeet

Page {
    id: page
    title: qsTr("Server files")

    Component.onCompleted: {
        if (Session.loggedIn)
            Explorer.refresh()
    }

    FileDialog {
        id: localFileDialog
        title: qsTr("Open local media")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("Media (*.mp4 *.mkv *.webm *.mov *.avi *.mp3 *.wav *.flac *.m4a)"),
            qsTr("All files (*)")
        ]
        onAccepted: {
            Explorer.openMedia(selectedFile)
            page.StackView.view.push(Qt.resolvedUrl("PlayerPage.qml"))
        }
    }

    function formatBytes(n) {
        const v = Number(n)
        if (!isFinite(v) || v <= 0)
            return ""
        if (v < 1024)
            return v + " B"
        if (v < 1024 * 1024)
            return (v / 1024).toFixed(1) + " KB"
        return (v / (1024 * 1024)).toFixed(1) + " MB"
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: qsTr("Back")
                onClicked: page.StackView.view.pop()
            }
            ToolButton {
                text: qsTr("Up")
                enabled: Explorer.canGoUp && !Explorer.busy
                onClicked: Explorer.goUp()
            }
            Label {
                Layout.fillWidth: true
                text: Explorer.currentPath
                elide: Text.ElideMiddle
            }
            BusyIndicator {
                running: Explorer.busy
                visible: running
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
            }
            ToolButton {
                text: qsTr("Refresh")
                enabled: !Explorer.busy
                onClicked: Explorer.refresh()
            }
            ToolButton {
                text: qsTr("Local…")
                onClicked: localFileDialog.open()
            }
        }
    }

    ListView {
        id: fileList
        anchors.fill: parent
        clip: true
        model: Explorer.files

        delegate: ItemDelegate {
            id: row
            required property var model
            required property int index
            width: ListView.view.width
            height: 56

            contentItem: RowLayout {
                spacing: 12
                Label {
                    text: model.isDir ? "📁" : (model.isPlayable ? "🎬" : "📄")
                    font.pixelSize: 18
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        Layout.fillWidth: true
                        text: model.name
                        elide: Text.ElideMiddle
                        font.bold: model.isDir
                    }
                    Label {
                        visible: !model.isDir
                        text: (model.mime || "") + (model.size > 0 ? "  ·  " + page.formatBytes(model.size) : "")
                        opacity: 0.65
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
                Label {
                    visible: model.isPlayable
                    text: qsTr("Play")
                    color: palette.highlight
                    font.pixelSize: 12
                }
            }

            onClicked: {
                const wasDir = model.isDir
                Explorer.openEntry(index)
                if (!wasDir)
                    page.StackView.view.push(Qt.resolvedUrl("PlayerPage.qml"))
            }
        }

        Label {
            anchors.centerIn: parent
            visible: fileList.count === 0 && !Explorer.busy
            width: parent.width * 0.8
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("No files here.\nOn the server admin window, choose a Hosted folder and Start.")
            opacity: 0.6
        }
    }

    Connections {
        target: Explorer
        function onErrorOccurred(message) {
            errorPopup.text = message
            errorPopup.open()
        }
    }

    Dialog {
        id: errorPopup
        property alias text: errorLabel.text
        anchors.centerIn: parent
        title: qsTr("Files")
        standardButtons: Dialog.Ok
        Label {
            id: errorLabel
            wrapMode: Text.Wrap
            width: 280
        }
    }
}
