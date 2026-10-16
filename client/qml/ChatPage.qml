import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMeet

Page {
    id: page
    title: Chat.activePeerName

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: qsTr("Back")
                onClicked: page.StackView.view.pop()
            }
            Label {
                text: page.title
                Layout.fillWidth: true
            }
            ToolButton {
                text: qsTr("Voice call")
                onClicked: page.StackView.view.push(Qt.resolvedUrl("CallView.qml"))
            }
            ToolButton {
                text: qsTr("Video call")
                onClicked: page.StackView.view.push(Qt.resolvedUrl("CallView.qml"))
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Send file")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Chat.messages
            delegate: Label {
                required property var model
                width: ListView.view.width
                horizontalAlignment: model.fromMe ? Text.AlignRight : Text.AlignLeft
                text: model.text
                wrapMode: Text.Wrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Attach")
                onClicked: fileDialog.open()
            }
            TextField {
                id: input
                Layout.fillWidth: true
                placeholderText: qsTr("Type a message")
                onAccepted: {
                    Chat.sendText(text)
                    clear()
                }
            }
            Button {
                text: qsTr("Send")
                onClicked: {
                    Chat.sendText(input.text)
                    input.clear()
                }
            }
        }
    }
}
