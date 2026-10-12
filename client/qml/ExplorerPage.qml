import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet

Page {
    id: page
    title: qsTr("Server files")

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: qsTr("Back")
                onClicked: page.StackView.view.pop()
            }
            ToolButton {
                text: qsTr("Up")
                enabled: Explorer.canGoUp
            }
            Label {
                Layout.fillWidth: true
                text: Explorer.currentPath.length > 0 ? Explorer.currentPath : "/"
                elide: Text.ElideMiddle
            }
            ToolButton {
                text: qsTr("Refresh")
            }
        }
    }

    ListView {
        anchors.fill: parent
        clip: true
        model: Explorer.files
        delegate: ItemDelegate {
            required property var model
            width: ListView.view.width
            text: (model.isDir ? "[DIR] " : "") + model.name
            onClicked: {
                if (!model.isDir)
                    page.StackView.view.push(Qt.resolvedUrl("PlayerPage.qml"))
            }
        }
    }

    Button {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        text: qsTr("Open player (demo)")
        onClicked: page.StackView.view.push(Qt.resolvedUrl("PlayerPage.qml"))
    }
}
