import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet

Page {
    id: page
    title: qsTr("Sign in")

    property bool registerMode: false

    Connections {
        target: Session
        function onSessionChanged() {
            if (Session.loggedIn)
                page.StackView.view.replace(Qt.resolvedUrl("HomePage.qml"))
        }
        function onLoginFailed(reason) {
            // errorString property already updated
        }
    }

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
            onEditingFinished: Session.serverHost = text.trim()
        }
        TextField {
            id: userField
            Layout.fillWidth: true
            placeholderText: qsTr("Username")
        }
        TextField {
            id: displayField
            Layout.fillWidth: true
            visible: page.registerMode
            placeholderText: qsTr("Display name")
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

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: Session.busy
            visible: running
        }

        CheckBox {
            text: qsTr("Create new account")
            checked: page.registerMode
            onToggled: page.registerMode = checked
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                Layout.fillWidth: true
                enabled: !Session.busy
                text: page.registerMode ? qsTr("Register") : qsTr("Sign in")
                onClicked: {
                    Session.serverHost = hostField.text.trim()
                    if (page.registerMode)
                        Session.registerUser(userField.text.trim(), displayField.text.trim(), passwordField.text)
                    else
                        Session.login(userField.text.trim(), passwordField.text)
                }
            }
        }
    }
}
