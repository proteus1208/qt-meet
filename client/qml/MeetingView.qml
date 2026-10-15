import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet
import "components"

Page {
    id: page
    objectName: "MeetingView"
    title: Call.meetingTitle.length ? Call.meetingTitle : qsTr("Meeting")

    Connections {
        target: Call
        function onStateChanged() {
            if (Call.state === 0) // Idle
                page.StackView.view.pop()
        }
        function onErrorOccurred(message) {
            errorLabel.text = message
        }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Label {
                Layout.leftMargin: 12
                Layout.fillWidth: true
                text: Call.meetingCode.length
                      ? qsTr("Meeting code: %1").arg(Call.meetingCode)
                      : qsTr("Meeting")
            }
            Label {
                visible: Call.screenSharing
                text: qsTr("Sharing screen")
                color: "#8ab4f8"
            }
            Label {
                id: errorLabel
                color: "#fbbc04"
                visible: text.length > 0
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
        cellWidth: width / Math.max(1, Math.ceil(Math.sqrt(Math.max(1, count))))
        cellHeight: cellWidth * 9 / 16
        model: Call.participants
        delegate: VideoTile {
            id: tile
            required property string displayName
            required property bool audioOn
            required property bool speaking
            required property bool isLocal
            required property var userId

            width: grid.cellWidth - 6
            height: grid.cellHeight - 6
            displayName: tile.isLocal && Call.screenSharing
                         ? qsTr("%1 (screen)").arg(tile.displayName)
                         : tile.displayName
            audioOn: tile.audioOn
            speaking: tile.speaking

            Component.onCompleted: {
                if (tile.isLocal)
                    Call.attachLocalPreview(tile.videoSink)
                else if (tile.userId)
                    Call.attachVideoSink(tile.userId, tile.videoSink)
            }
            Component.onDestruction: {
                if (!tile.isLocal && tile.userId)
                    Call.detachVideoSink(tile.userId)
            }
        }
    }

    CallMediaControls {
        id: controls
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        showLeave: true
        leaveText: qsTr("Leave")
        onLeaveRequested: Call.leaveMeeting()
    }
}
