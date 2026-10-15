import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    title: qsTr("Join meeting")
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel

    property alias code: codeField.text
    property alias withCamera: cameraBox.checked

    signal joinRequested(string code, bool video)

    onAccepted: dialog.joinRequested(codeField.text.trim(), cameraBox.checked)
    onOpened: {
        codeField.clear()
        cameraBox.checked = true
        codeField.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label { text: qsTr("Enter the meeting code") }
        TextField {
            id: codeField
            Layout.preferredWidth: 260
            placeholderText: qsTr("e.g. K7Q4-M2XP")
        }
        CheckBox {
            id: cameraBox
            text: qsTr("Join with camera")
            checked: true
        }
    }
}
