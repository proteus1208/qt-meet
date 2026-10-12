import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet
import "components"

Page {
    id: page
    title: qsTr("Call")

    Rectangle {
        anchors.fill: parent
        color: "#202124"
    }

    VideoTile {
        id: remoteTile
        anchors.fill: parent
        anchors.bottomMargin: controls.height + 16
        displayName: Call.peerName
    }

    VideoTile {
        id: localTile
        width: 220
        height: 124
        anchors.right: parent.right
        anchors.bottom: controls.top
        anchors.margins: 12
        displayName: qsTr("You")
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
            text: qsTr("Hang up")
            onClicked: page.StackView.view.pop()
        }
    }
}
