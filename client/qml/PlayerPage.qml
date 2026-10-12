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
                onClicked: page.StackView.view.pop()
            }
            Label {
                Layout.fillWidth: true
                text: Explorer.player.source.toString()
                elide: Text.ElideMiddle
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
        Component.onCompleted: Explorer.player.videoSink = videoOutput.videoSink
    }

    RowLayout {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8

        Button {
            text: Explorer.player.playing ? qsTr("Pause") : qsTr("Play")
            onClicked: Explorer.player.playing ? Explorer.player.pause() : Explorer.player.play()
        }
        Slider {
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, Explorer.player.duration)
            value: Explorer.player.position
            enabled: Explorer.player.seekable
            onMoved: Explorer.player.seek(value)
        }
        Slider {
            Layout.preferredWidth: 100
            from: 0
            to: 1
            value: Explorer.player.volume
            onMoved: Explorer.player.volume = value
        }
        Label {
            text: Explorer.player.hardwareDecoding ? qsTr("GPU") : qsTr("CPU")
        }
    }
}
