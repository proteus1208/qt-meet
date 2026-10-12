import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet

Page {
    id: page
    title: qsTr("Contacts")

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Label {
                text: Session.displayName.length > 0 ? Session.displayName : qsTr("Contacts")
                Layout.leftMargin: 12
                Layout.fillWidth: true
            }
            ToolButton {
                text: qsTr("Files")
                onClicked: page.StackView.view.push(Qt.resolvedUrl("ExplorerPage.qml"))
            }
            ToolButton {
                text: qsTr("New meeting")
                onClicked: page.StackView.view.push(Qt.resolvedUrl("MeetingView.qml"))
            }
            ToolButton {
                text: qsTr("Join meeting")
                onClicked: joinDialog.open()
            }
            ToolButton {
                text: qsTr("Sign out")
                onClicked: page.StackView.view.replace(Qt.resolvedUrl("LoginPage.qml"))
            }
        }
    }

    JoinMeetingDialog {
        id: joinDialog
        anchors.centerIn: parent
        onJoinRequested: (code) => page.StackView.view.push(Qt.resolvedUrl("MeetingView.qml"))
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        TextField {
            Layout.fillWidth: true
            placeholderText: qsTr("Search users...")
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Chat.searchResults
            delegate: ItemDelegate {
                required property var model
                width: ListView.view.width
                text: model.displayName
                onClicked: page.StackView.view.push(Qt.resolvedUrl("ChatPage.qml"))
            }
        }

        Button {
            text: qsTr("Open chat (demo)")
            onClicked: page.StackView.view.push(Qt.resolvedUrl("ChatPage.qml"))
        }
    }
}
