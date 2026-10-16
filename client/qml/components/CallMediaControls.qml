import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet
import "../theme"

Item {
    id: root
    width: Math.min(bar.implicitWidth, Overlay.overlay ? Overlay.overlay.width - 24 : bar.implicitWidth)
    height: bar.implicitHeight

    property bool showLeave: false
    property bool watchMode: false
    property string leaveText: qsTr("Hang up")
    signal leaveRequested()

    readonly property Theme theme: Theme {}

    Rectangle {
        anchors.fill: bar
        anchors.margins: -10
        radius: theme.radius
        color: "#E6141824"
        border.color: theme.border
        border.width: 1
    }

    RowLayout {
        id: bar
        spacing: 8

        Button {
            visible: !root.watchMode
            text: Call.micEnabled ? qsTr("Mic on") : qsTr("Mic off")
            checked: Call.micEnabled
            checkable: true
            onClicked: Call.micEnabled = !Call.micEnabled
        }
        Button {
            text: Call.speakerEnabled ? qsTr("Speaker") : qsTr("Speaker off")
            checked: Call.speakerEnabled
            checkable: true
            onClicked: Call.speakerEnabled = !Call.speakerEnabled
        }
        Button {
            visible: !root.watchMode
            text: Call.cameraEnabled ? qsTr("Camera") : qsTr("Cam off")
            checked: Call.cameraEnabled
            checkable: true
            onClicked: Call.cameraEnabled = !Call.cameraEnabled
        }
        Button {
            visible: !root.watchMode
            text: Call.screenSharing ? qsTr("Stop share") : qsTr("Share screen")
            checked: Call.screenSharing
            checkable: true
            onClicked: Call.screenSharing = !Call.screenSharing
        }
        Button {
            text: root.watchMode ? qsTr("Audio") : qsTr("Audio / video")
            onClicked: deviceDialog.open()
        }
        Button {
            visible: root.showLeave
            text: root.leaveText
            onClicked: root.leaveRequested()
            palette.buttonText: theme.danger
        }
    }

    Dialog {
        id: deviceDialog
        title: qsTr("Devices & volume")
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        standardButtons: Dialog.Close
        width: Math.min(440, parent ? parent.width - 40 : 440)

        onAboutToShow: Call.refreshDevices()

        ColumnLayout {
            spacing: 12
            width: deviceDialog.availableWidth

            Label {
                text: qsTr("Video source")
                font.bold: true
            }
            Label {
                text: qsTr("Camera or entire screen")
                color: theme.textMuted
                font.pixelSize: 12
            }
            ComboBox {
                Layout.fillWidth: true
                model: Call.videoDevices
                currentIndex: Call.videoIndex
                enabled: count > 0
                onActivated: Call.videoIndex = index
            }

            Label {
                text: qsTr("Microphone")
                font.bold: true
            }
            ComboBox {
                Layout.fillWidth: true
                model: Call.microphoneDevices
                currentIndex: Call.microphoneIndex
                enabled: count > 0
                onActivated: Call.microphoneIndex = index
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Mic level"); Layout.preferredWidth: 90 }
                Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: Call.micVolume
                    onMoved: Call.micVolume = value
                }
                Label {
                    text: Math.round(Call.micVolume * 100) + "%"
                    Layout.preferredWidth: 40
                    horizontalAlignment: Text.AlignRight
                }
            }

            Label {
                text: qsTr("Bandwidth")
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: theme.textMuted
                font.pixelSize: 12
                text: root.watchMode
                      ? qsTr("Preferred channel quality (resolution, fps, bitrate) sent to the server.")
                      : qsTr("Your upload quality: resolution, frame rate, and audio bitrate.")
            }
            ComboBox {
                Layout.fillWidth: true
                model: Call.bandwidthLabels
                currentIndex: Call.bandwidthIndex
                onActivated: Call.bandwidthIndex = index
            }

            Label {
                text: qsTr("Speaker (system output)")
                font.bold: true
            }
            ComboBox {
                Layout.fillWidth: true
                model: Call.speakerDevices
                currentIndex: Call.speakerIndex
                enabled: count > 0
                onActivated: Call.speakerIndex = index
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Volume"); Layout.preferredWidth: 90 }
                Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: Call.speakerVolume
                    onMoved: Call.speakerVolume = value
                }
                Label {
                    text: Math.round(Call.speakerVolume * 100) + "%"
                    Layout.preferredWidth: 40
                    horizontalAlignment: Text.AlignRight
                }
            }

            Label {
                visible: Call.screenSharing
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                text: qsTr("Screen is your current video source. Pick a camera above to stop sharing.")
                opacity: 0.75
            }
        }
    }
}
