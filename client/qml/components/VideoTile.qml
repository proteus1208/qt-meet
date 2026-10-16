import QtQuick
import QtQuick.Controls
import QtMultimedia
import "../theme"

Rectangle {
    id: tile

    property string displayName
    property bool audioOn: true
    property bool speaking: false
    property bool isLocal: false
    property bool screenShare: false
    property alias videoSink: output.videoSink
    /// Width / height of the current frame (16:9 until the first frame arrives).
    readonly property real contentAspect: {
        const w = output.sourceRect.width
        const h = output.sourceRect.height
        if (w > 0 && h > 0)
            return w / h
        return 16 / 9
    }

    readonly property Theme theme: Theme {}

    color: theme.surfaceAlt
    radius: theme.radius
    border.width: speaking ? 2 : 1
    border.color: speaking ? theme.success : theme.border
    clip: true

    VideoOutput {
        id: output
        anchors.fill: parent
        anchors.margins: 1
        // Keep correct frame ratio (letterbox/pillarbox); do not crop ultrawide screens.
        fillMode: VideoOutput.PreserveAspectFit
    }

    Rectangle {
        anchors.fill: parent
        visible: !output.videoSink || output.sourceRect.width === 0
        color: theme.surface
        opacity: 0.35
    }

    Label {
        anchors.centerIn: parent
        visible: output.sourceRect.width === 0
        text: tile.displayName.length ? tile.displayName.charAt(0).toUpperCase() : "?"
        color: theme.textMuted
        font.pixelSize: Math.max(28, Math.min(tile.width, tile.height) * 0.22)
        font.bold: true
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 36
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: "#CC0B0E17" }
        }
    }

    Row {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 10
        spacing: 6

        Label {
            text: {
                if (tile.screenShare)
                    return qsTr("%1 · screen").arg(tile.displayName)
                return tile.displayName
            }
            color: theme.text
            font.pixelSize: 13
            font.bold: true
        }
        Label {
            visible: !tile.audioOn
            text: qsTr("muted")
            color: theme.danger
            font.pixelSize: 12
        }
    }

    Rectangle {
        visible: tile.isLocal
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        width: localLabel.implicitWidth + 12
        height: 22
        radius: 6
        color: theme.accentSoft
        Label {
            id: localLabel
            anchors.centerIn: parent
            text: qsTr("You")
            color: theme.accent
            font.pixelSize: 11
        }
    }
}
