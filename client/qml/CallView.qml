import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet
import "components"
import "theme"

Page {
    id: page
    title: qsTr("Call")

    readonly property Theme theme: Theme {}
    readonly property bool inCall: Call.state === 3
    readonly property bool incoming: Call.state === 2
    readonly property bool outgoing: Call.state === 1

    Connections {
        target: Call
        function onStateChanged() {
            if (Call.state === 0)
                page.StackView.view.pop()
            else if (Call.peerId !== 0)
                Call.attachVideoSink(Call.peerId, remoteTile.videoSink)
            if (Call.state === 3 && Call.peerId !== 0)
                Chat.openConversationWith(Call.peerId, Call.peerName || qsTr("Call"))
        }
        function onCallEnded(reason) {
            endLabel.text = qsTr("Call ended: %1").arg(reason)
            chatPanel.open = false
        }
        function onErrorOccurred(message) {
            endLabel.text = message
        }
        function onScreenSharingChanged() {
            Call.attachLocalPreview(localTile.videoSink)
        }
    }

    Component.onCompleted: {
        if (Call.state === 3 && Call.peerId !== 0)
            Chat.openConversationWith(Call.peerId, Call.peerName || qsTr("Call"))
    }

    Rectangle {
        anchors.fill: parent
        color: theme.bg
    }

    VideoTile {
        id: remoteTile
        anchors.fill: parent
        anchors.margins: 12
        anchors.rightMargin: chatPanel.open ? chatPanel.widthHint + 20 : 12
        anchors.bottomMargin: controls.height + 28
        displayName: Call.peerName.length ? Call.peerName : qsTr("Remote")
        Component.onCompleted: {
            if (Call.peerId !== 0)
                Call.attachVideoSink(Call.peerId, remoteTile.videoSink)
        }
    }

    VideoTile {
        id: localTile
        width: Math.min(240, page.width * 0.28)
        height: Math.max(90, width / Math.max(0.2, contentAspect))
        anchors.right: parent.right
        anchors.rightMargin: chatPanel.open ? chatPanel.widthHint + 28 : 16
        anchors.bottom: controls.top
        anchors.margins: 16
        isLocal: true
        screenShare: Call.screenSharing
        displayName: Call.screenSharing ? qsTr("Your screen") : qsTr("You")
        Component.onCompleted: Call.attachLocalPreview(localTile.videoSink)
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 16
        visible: page.incoming || page.outgoing
        width: inviteCol.implicitWidth + 40
        height: inviteCol.implicitHeight + 28
        radius: theme.radius
        color: "#E6141824"
        border.color: theme.border

        Column {
            id: inviteCol
            anchors.centerIn: parent
            spacing: 14
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                color: theme.text
                font.pixelSize: 20
                font.bold: true
                text: page.incoming
                      ? qsTr("Incoming call from %1").arg(Call.peerName || qsTr("Unknown"))
                      : qsTr("Calling %1…").arg(Call.peerName || qsTr("Unknown"))
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12
                visible: page.incoming
                Button {
                    text: qsTr("Accept")
                    onClicked: Call.acceptCall()
                }
                Button {
                    text: qsTr("Reject")
                    onClicked: Call.rejectCall()
                }
            }
        }
    }

    Label {
        id: endLabel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 16
        color: "#FBBF24"
        visible: text.length > 0 && !page.incoming && !page.outgoing
    }

    Label {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 16
        visible: page.inCall && Call.peerName.length > 0
        text: Call.peerName
        color: theme.text
        font.pixelSize: 16
        font.bold: true
    }

    CallMediaControls {
        id: controls
        z: 3
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        showLeave: true
        showChat: page.inCall || page.outgoing
        chatOpen: chatPanel.open
        leaveText: qsTr("Hang up")
        onLeaveRequested: Call.hangup()
        onChatRequested: chatPanel.open = !chatPanel.open
    }

    InCallChatPanel {
        id: chatPanel
        anchors.fill: parent
        title: Call.peerName.length ? Call.peerName : qsTr("Chat")
        readonly property real widthHint: Math.min(360, page.width * 0.92)
    }
}
