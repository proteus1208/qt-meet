import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet
import "components"
import "theme"

Page {
    id: page
    objectName: "MeetingView"
    title: Call.meetingTitle.length ? Call.meetingTitle : qsTr("Meeting")

    readonly property Theme theme: Theme {}

    Connections {
        target: Call
        function onStateChanged() {
            if (Call.state === 0)
                page.StackView.view.pop()
        }
        function onErrorOccurred(message) {
            errorLabel.text = message
        }
        function onScreenSharingChanged() {
            // Force local tiles to re-bind preview after capture swap.
            grid.forceLayout()
        }
    }

    header: ToolBar {
        background: Rectangle { color: theme.surface }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    text: Call.meetingTitle.length ? Call.meetingTitle : qsTr("Meeting")
                    color: theme.text
                    font.bold: true
                    font.pixelSize: 15
                }
                Label {
                    visible: Call.meetingCode.length > 0
                    text: qsTr("Code %1").arg(Call.meetingCode)
                    color: theme.textMuted
                    font.pixelSize: 12
                }
            }

            Label {
                visible: Call.screenSharing
                text: qsTr("Sharing screen")
                color: theme.accent
                font.pixelSize: 12
            }

            Label {
                id: errorLabel
                color: "#FBBF24"
                visible: text.length > 0
                elide: Text.ElideRight
                Layout.maximumWidth: 220
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: theme.bg
    }

    GridView {
        id: grid
        anchors.fill: parent
        anchors.margins: 10
        anchors.bottomMargin: controls.height + 24
        clip: true
        cellWidth: {
            const n = Math.max(1, count)
            const cols = Math.ceil(Math.sqrt(n))
            return width / cols
        }
        cellHeight: cellWidth * 9 / 16
        model: Call.participants
        delegate: Item {
            id: cell
            required property string displayName
            required property bool audioOn
            required property bool speaking
            required property bool isLocal
            required property var userId

            width: grid.cellWidth
            height: grid.cellHeight

            VideoTile {
                id: tile
                anchors.fill: parent
                anchors.margins: 5
                displayName: cell.displayName
                audioOn: cell.audioOn
                speaking: cell.speaking
                isLocal: cell.isLocal
                screenShare: cell.isLocal && Call.screenSharing

                Component.onCompleted: {
                    if (cell.isLocal)
                        Call.attachLocalPreview(tile.videoSink)
                    else if (cell.userId)
                        Call.attachVideoSink(cell.userId, tile.videoSink)
                }
                Component.onDestruction: {
                    if (!cell.isLocal && cell.userId)
                        Call.detachVideoSink(cell.userId)
                }

                Connections {
                    target: Call
                    function onScreenSharingChanged() {
                        if (cell.isLocal)
                            Call.attachLocalPreview(tile.videoSink)
                    }
                }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: grid.count === 0
        text: qsTr("Waiting for participants…")
        color: theme.textMuted
    }

    CallMediaControls {
        id: controls
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        showLeave: true
        leaveText: qsTr("Leave")
        onLeaveRequested: Call.leaveMeeting()
    }
}
