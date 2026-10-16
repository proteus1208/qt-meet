import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import QtMeet

Page {
    id: page
    title: qsTr("Player")

    readonly property bool isPlaying: player.playbackState === MediaPlayer.PlayingState

    // Qt Multimedia player — correct stack for file/HTTP playback (not custom FFmpeg→QVideoSink).
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
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: qsTr("Back")
                onClicked: {
                    player.stop()
                    Explorer.clearMedia()
                    page.StackView.view.pop()
                }
            }
            Label {
                Layout.fillWidth: true
                text: Explorer.mediaUrl.toString()
                elide: Text.ElideMiddle
            }
            Label {
                text: {
                    switch (player.mediaStatus) {
                    case MediaPlayer.LoadingMedia: return qsTr("Loading…")
                    case MediaPlayer.BufferingMedia: return qsTr("Buffering…")
                    case MediaPlayer.InvalidMedia: return qsTr("Invalid")
                    default: return page.isPlaying ? qsTr("Playing") : qsTr("Paused")
                    }
                }
                opacity: 0.8
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
        anchors.bottomMargin: controls.height
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
        color: "white"
        text: qsTr("Loading…")
        opacity: 0.7
    }

    function formatMs(ms) {
        const s = Math.floor(Number(ms) / 1000)
        const m = Math.floor(s / 60)
        const r = s % 60
        return m + ":" + String(r).padStart(2, "0")
    }

    RowLayout {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        spacing: 8

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
            color: "white"
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
            color: "white"
        }
        Slider {
            id: volumeSlider
            Layout.preferredWidth: 100
            from: 0
            to: 1
            value: 1
        }
    }

    Component.onDestruction: player.stop()
}
