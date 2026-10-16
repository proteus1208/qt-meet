import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet
import "components"
import "theme"

Page {
    id: page
    objectName: "ChannelWatchView"
    title: Call.channelName.length ? Call.channelName : qsTr("Live channel")

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
                    text: Call.channelName.length ? Call.channelName : qsTr("Live channel")
                    color: theme.text
                    font.bold: true
                    font.pixelSize: 15
                }
                Label {
                    text: qsTr("Live · IPTV")
                    color: theme.textMuted
                    font.pixelSize: 12
                }
            }

            Label {
                id: errorLabel
                color: "#FBBF24"
                visible: text.length > 0
                elide: Text.ElideRight
                Layout.maximumWidth: 220
            }

            Button {
                text: qsTr("Leave")
                onClicked: Call.leaveChannel()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: theme.bg
    }

    Item {
        anchors.fill: parent
        anchors.margins: 12
        anchors.bottomMargin: controls.height + 24

        Repeater {
            model: Call.participants
            delegate: Item {
                id: cell
                anchors.fill: parent
                required property string displayName
                required property bool audioOn
                required property bool speaking
                required property bool isLocal
                required property var userId
                visible: !cell.isLocal

                VideoTile {
                    id: tile
                    anchors.fill: parent
                    displayName: cell.displayName
                    audioOn: cell.audioOn
                    speaking: cell.speaking
                    isLocal: false

                    Component.onCompleted: {
                        if (cell.userId)
                            Call.attachVideoSink(cell.userId, tile.videoSink)
                    }
                    Component.onDestruction: {
                        if (cell.userId)
                            Call.detachVideoSink(cell.userId)
                    }
                }
            }
        }
    }

    CallMediaControls {
        id: controls
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        watchMode: true
        showLeave: true
        leaveText: qsTr("Leave")
        onLeaveRequested: Call.leaveChannel()
    }
}
