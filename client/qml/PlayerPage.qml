import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import QtMeet
import "theme"

Page {
    id: page
    title: qsTr("Player")

    readonly property Theme theme: Theme {}
    readonly property bool isPlaying: player.playbackState === MediaPlayer.PlayingState

    MediaPlayer {
        id: player
        source: Explorer.mediaUrl
        videoOutput: videoOutput
        audioOutput: audioOut
        onErrorOccurred: function(error, errorString) {
            errorLabel.text = errorString || qsTr("Playback error")
        }
        onSourceChanged: {
            errorLabel.text = ""
            if (source.toString().length > 0)
                play()
        }
        Component.onCompleted: {
            if (source.toString().length > 0)
                play()
        }
    }

    AudioOutput {
        id: audioOut
        volume: volumeSlider.value
    }

    header: ToolBar {
        background: Rectangle { color: theme.surface }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 12
            ToolButton {
                text: qsTr("Back")
                onClicked: {
                    player.stop()
                    Explorer.clearMedia()
                    page.StackView.view.pop()
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: {
                        const u = Explorer.mediaUrl
                        const name = u.fileName || u.toString()
                        return name.length ? name : qsTr("Player")
                    }
                    elide: Text.ElideMiddle
                    color: theme.text
                    font.bold: true
                }
                Label {
                    text: {
                        switch (player.mediaStatus) {
                        case MediaPlayer.LoadingMedia: return qsTr("Loading…")
                        case MediaPlayer.BufferingMedia: return qsTr("Buffering…")
                        case MediaPlayer.InvalidMedia: return qsTr("Invalid media")
                        case MediaPlayer.EndOfMedia: return qsTr("Ended")
                        default: return page.isPlaying ? qsTr("Playing") : qsTr("Paused")
                        }
                    }
                    color: theme.textMuted
                    font.pixelSize: 12
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        anchors.bottomMargin: controls.height + 8
        fillMode: VideoOutput.PreserveAspectFit
    }

    Label {
        id: errorLabel
        anchors.centerIn: parent
        visible: text.length > 0
        color: "white"
        width: parent.width * 0.8
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
    }

    Label {
        anchors.centerIn: parent
        visible: errorLabel.text.length === 0
                 && player.mediaStatus === MediaPlayer.LoadingMedia
        color: theme.textMuted
        text: qsTr("Loading…")
    }

    function formatMs(ms) {
        const s = Math.floor(Number(ms) / 1000)
        const m = Math.floor(s / 60)
        const r = s % 60
        return m + ":" + String(r).padStart(2, "0")
    }

    Rectangle {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: controlsRow.implicitHeight + 20
        color: "#E6141824"

        RowLayout {
            id: controlsRow
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            Button {
                text: page.isPlaying ? qsTr("Pause") : qsTr("Play")
                onClicked: page.isPlaying ? player.pause() : player.play()
            }
            Button {
                text: qsTr("Stop")
                onClicked: player.stop()
            }
            Label {
                text: page.formatMs(player.position)
                color: theme.text
            }
            Slider {
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, player.duration)
                value: player.position
                enabled: player.seekable
                onMoved: player.position = value
            }
            Label {
                text: page.formatMs(player.duration)
                color: theme.textMuted
            }
            Label {
                text: qsTr("Vol")
                color: theme.textMuted
            }
            Slider {
                id: volumeSlider
                Layout.preferredWidth: 110
                from: 0
                to: 1
                value: 1
            }
        }
    }

    Component.onDestruction: player.stop()
}
