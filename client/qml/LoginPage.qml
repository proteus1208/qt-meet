import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet

Page {
    id: page
    title: qsTr("Sign in")

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 360)
        spacing: 10

        Label {
            text: qsTr("QtMeet")
            font.pixelSize: 28
            Layout.alignment: Qt.AlignHCenter
        }

        TextField {
            id: hostField
            Layout.fillWidth: true
            placeholderText: qsTr("Server address")
            text: Session.serverHost
        }
        TextField {
            id: userField
            Layout.fillWidth: true
            placeholderText: qsTr("Username")
        }
        TextField {
            id: passwordField
            Layout.fillWidth: true
            placeholderText: qsTr("Password")
            echoMode: TextInput.Password
        }

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: Session.errorString
            color: "red"
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                Layout.fillWidth: true
                text: qsTr("Sign in")
                onClicked: page.StackView.view.replace(Qt.resolvedUrl("ContactsPage.qml"))
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Register")
                onClicked: page.StackView.view.replace(Qt.resolvedUrl("ContactsPage.qml"))
            }
        }
    }
}
