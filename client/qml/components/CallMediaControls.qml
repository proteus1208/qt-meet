import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet

Item {
    id: root
    width: bar.implicitWidth
    height: bar.implicitHeight

    property bool showLeave: false
    property string leaveText: qsTr("Hang up")
    signal leaveRequested()

    RowLayout {
        id: bar
        spacing: 8

        Button {
            text: Call.micEnabled ? qsTr("Mute mic") : qsTr("Unmute mic")
            onClicked: Call.micEnabled = !Call.micEnabled
        }
        Button {
            text: Call.speakerEnabled ? qsTr("Mute speaker") : qsTr("Unmute speaker")
            onClicked: Call.speakerEnabled = !Call.speakerEnabled
        }
        Button {
            text: Call.cameraEnabled ? qsTr("Camera off") : qsTr("Camera on")
            onClicked: Call.cameraEnabled = !Call.cameraEnabled
        }
        Button {
            text: Call.screenSharing ? qsTr("Stop share") : qsTr("Share screen")
            onClicked: Call.screenSharing = !Call.screenSharing
        }
        Button {
            text: qsTr("Devices…")
            onClicked: deviceDialog.open()
        }
        Button {
            visible: root.showLeave
            text: root.leaveText
            onClicked: root.leaveRequested()
        }
    }

    Dialog {
        id: deviceDialog
        title: qsTr("Audio & video devices")
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        standardButtons: Dialog.Close
        width: Math.min(420, parent ? parent.width - 40 : 420)

        onAboutToShow: Call.refreshDevices()

        ColumnLayout {
            spacing: 10
            width: deviceDialog.availableWidth

            Label { text: qsTr("Camera") }
            ComboBox {
                Layout.fillWidth: true
                model: Call.cameraDevices
                currentIndex: Call.cameraIndex
                enabled: count > 0 && !Call.screenSharing
                onActivated: Call.cameraIndex = index
            }

            Label { text: qsTr("Microphone") }
            ComboBox {
                Layout.fillWidth: true
                model: Call.microphoneDevices
                currentIndex: Call.microphoneIndex
                enabled: count > 0
                onActivated: Call.microphoneIndex = index
            }

            Label { text: qsTr("Speaker") }
            ComboBox {
                Layout.fillWidth: true
                model: Call.speakerDevices
                currentIndex: Call.speakerIndex
                enabled: count > 0
                onActivated: Call.speakerIndex = index
            }

            Label {
                visible: Call.screenSharing
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                text: qsTr("Camera selection is locked while sharing the screen.")
                opacity: 0.7
            }
        }
    }
}
