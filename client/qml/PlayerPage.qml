import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import QtMeet

Page {
    id: page
    title: qsTr("Player")

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: qsTr("Back")
                onClicked: {
                    Explorer.player.pause()
                    page.StackView.view.pop()
                }
            }
            Label {
                Layout.fillWidth: true
                text: Explorer.player.source.toString()
                elide: Text.ElideMiddle
            }
            Label {
                text: Explorer.player.hardwareDecoding ? qsTr("GPU") : qsTr("CPU")
                opacity: 0.8
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        anchors.bottomMargin: controls.height
        Component.onCompleted: {
            // Bind the sink before play so the first video frames are not dropped.
            Explorer.player.videoSink = videoOutput.videoSink
            page.tryPlay()
        }
        Component.onDestruction: {
            if (Explorer.player.videoSink === videoOutput.videoSink)
                Explorer.player.videoSink = null
        }
    }

    function tryPlay() {
        if (Explorer.player.videoSink !== videoOutput.videoSink)
            return
        if (Explorer.player.source.toString().length === 0)
            return
        if (!Explorer.player.playing)
            Explorer.player.play()
    }

    Connections {
        target: Explorer.player
        function onSourceChanged() { page.tryPlay() }
        function onVideoSinkChanged() { page.tryPlay() }
    }

    Label {
        anchors.centerIn: parent
        visible: Explorer.player.errorString.length > 0
        color: "white"
        text: Explorer.player.errorString
        width: parent.width * 0.8
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
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
            text: Explorer.player.playing ? qsTr("Pause") : qsTr("Play")
            onClicked: Explorer.player.playing ? Explorer.player.pause() : Explorer.player.play()
        }
        Button {
            text: qsTr("Stop")
            onClicked: Explorer.player.stop()
        }
        Label {
            text: page.formatMs(Explorer.player.position)
            color: "white"
        }
        Slider {
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, Explorer.player.duration)
            value: Explorer.player.position
            enabled: Explorer.player.seekable
            onMoved: Explorer.player.seek(value)
        }
        Label {
            text: page.formatMs(Explorer.player.duration)
            color: "white"
        }
        Slider {
            Layout.preferredWidth: 100
            from: 0
            to: 1
            value: Explorer.player.volume
            onMoved: Explorer.player.volume = value
        }
    }

    Connections {
        target: Explorer.player
        function onEndOfMedia() {
            // stay on page; user can seek / replay
        }
    }
}
