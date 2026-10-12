import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet
import "components"

Page {
    id: page
    title: Call.meetingTitle

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Label {
                Layout.leftMargin: 12
                Layout.fillWidth: true
                text: qsTr("Meeting code: %1").arg(Call.meetingCode)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#202124"
    }

    GridView {
        id: grid
        anchors.fill: parent
        anchors.margins: 8
        anchors.bottomMargin: controls.height + 16
        clip: true
        cellWidth: width / Math.max(1, Math.ceil(Math.sqrt(count)))
        cellHeight: cellWidth * 9 / 16
        model: Call.participants
        delegate: VideoTile {
            required property var model
            width: grid.cellWidth - 6
            height: grid.cellHeight - 6
            displayName: model.displayName
            audioOn: model.audioOn
            speaking: model.speaking
        }
    }

    RowLayout {
        id: controls
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        spacing: 12

        Button {
            text: Call.micEnabled ? qsTr("Mute") : qsTr("Unmute")
        }
        Button {
            text: Call.cameraEnabled ? qsTr("Camera off") : qsTr("Camera on")
        }
        Button {
            text: qsTr("Leave")
            onClicked: page.StackView.view.pop()
        }
    }
}
