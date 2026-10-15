import QtQuick
import QtQuick.Controls
import QtMultimedia

Rectangle {
    id: tile

    property string displayName
    property bool audioOn: true
    property bool speaking: false
    property alias videoSink: output.videoSink

    color: "#3c4043"
    radius: 6
    border.width: speaking ? 3 : 0
    border.color: "#34a853"

    VideoOutput {
        id: output
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
    }

    Label {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 8
        color: "white"
        text: tile.displayName + (tile.audioOn ? "" : qsTr("  (muted)"))
    }
}
