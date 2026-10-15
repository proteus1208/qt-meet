import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet
import "components"

Page {
    id: page
    title: qsTr("Call")

    readonly property bool inCall: Call.state === 3 // InCall
    readonly property bool incoming: Call.state === 2 // Incoming
    readonly property bool outgoing: Call.state === 1 // Outgoing

    Connections {
        target: Call
        function onStateChanged() {
            if (Call.state === 0) // Idle
                page.StackView.view.pop()
            else if (Call.peerId !== 0)
                Call.attachVideoSink(Call.peerId, remoteTile.videoSink)
        }
        function onCallEnded(reason) {
            endLabel.text = qsTr("Call ended: %1").arg(reason)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#202124"
    }

    VideoTile {
        id: remoteTile
        anchors.fill: parent
        anchors.bottomMargin: controls.height + 16
        displayName: Call.peerName.length ? Call.peerName : qsTr("Remote")
        Component.onCompleted: {
            if (Call.peerId !== 0)
                Call.attachVideoSink(Call.peerId, remoteTile.videoSink)
        }
    }

    VideoTile {
        id: localTile
        width: 220
        height: 124
        anchors.right: parent.right
        anchors.bottom: controls.top
        anchors.margins: 12
        displayName: qsTr("You")
        Component.onCompleted: Call.attachLocalPreview(localTile.videoSink)
    }

    Column {
        anchors.centerIn: parent
        spacing: 12
        visible: page.incoming || page.outgoing
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "white"
            font.pixelSize: 20
            text: page.incoming ? qsTr("Incoming call from %1").arg(Call.peerName)
                                : qsTr("Calling %1…").arg(Call.peerName)
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

    Label {
        id: endLabel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 16
        color: "#fbbc04"
        visible: text.length > 0
    }

    RowLayout {
        id: controls
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        spacing: 12

        Button {
            text: Call.micEnabled ? qsTr("Mute") : qsTr("Unmute")
            onClicked: Call.micEnabled = !Call.micEnabled
        }
        Button {
            text: Call.cameraEnabled ? qsTr("Camera off") : qsTr("Camera on")
            onClicked: Call.cameraEnabled = !Call.cameraEnabled
        }
        Button {
            text: qsTr("Hang up")
            onClicked: Call.hangup()
        }
    }
}
