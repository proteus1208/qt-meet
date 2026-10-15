import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    title: qsTr("Join meeting")
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel

    signal joinRequested(string code)

    onAccepted: dialog.joinRequested(codeField.text.trim())
    onOpened: codeField.clear()

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
            text: qsTr("Join with camera")
            checked: true
        }
    }
}
