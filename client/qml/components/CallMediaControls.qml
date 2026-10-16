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

    function openMicMenu() {
        Call.refreshDevices()
        micMenu.open()
    }
    function openCamMenu() {
        Call.refreshDevices()
        camMenu.open()
    }
    function openSpeakerMenu() {
        Call.refreshDevices()
        speakerMenu.open()
    }

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
        spacing: 10

        // Mic: click = mute/unmute, ▾ = choose mic or system audio
        Row {
            visible: !root.watchMode
            spacing: 0

            Button {
                id: micBtn
                width: 108
                text: Call.micEnabled ? qsTr("Mic") : qsTr("Muted")
                checked: Call.micEnabled
                checkable: true
                onClicked: Call.micEnabled = !Call.micEnabled
                background: Rectangle {
                    radius: theme.radiusSm
                    color: Call.micEnabled ? theme.surfaceAlt : "#3F1D1D"
                    border.color: theme.border
                }
            }
            Button {
                width: 34
                text: "▾"
                onClicked: root.openMicMenu()
                background: Rectangle {
                    radius: theme.radiusSm
                    color: theme.surfaceHover
                    border.color: theme.border
                }
                Menu {
                    id: micMenu
                    y: -implicitHeight - 8
                    width: Math.max(240, micBtn.width + 34)
                    Instantiator {
                        model: Call.microphoneDevices
                        delegate: MenuItem {
                            text: modelData
                            checked: Call.microphoneIndex === index
                            checkable: true
                            onTriggered: {
                                Call.microphoneIndex = index
                                if (!Call.micEnabled)
                                    Call.micEnabled = true
                            }
                        }
                        onObjectAdded: function(i, obj) { micMenu.insertItem(i, obj) }
                        onObjectRemoved: function(i, obj) { micMenu.removeItem(obj) }
                    }
                }
            }
        }

        // Camera: click = on/off, ▾ = choose camera or screen (no separate share button)
        Row {
            visible: !root.watchMode
            spacing: 0

            Button {
                id: camBtn
                width: 108
                text: {
                    if (!Call.cameraEnabled)
                        return qsTr("Cam off")
                    return Call.screenSharing ? qsTr("Screen") : qsTr("Camera")
                }
                checked: Call.cameraEnabled
                checkable: true
                onClicked: Call.cameraEnabled = !Call.cameraEnabled
                background: Rectangle {
                    radius: theme.radiusSm
                    color: Call.cameraEnabled ? theme.surfaceAlt : "#3F1D1D"
                    border.color: theme.border
                }
            }
            Button {
                width: 34
                text: "▾"
                onClicked: root.openCamMenu()
                background: Rectangle {
                    radius: theme.radiusSm
                    color: theme.surfaceHover
                    border.color: theme.border
                }
                Menu {
                    id: camMenu
                    y: -implicitHeight - 8
                    width: Math.max(260, camBtn.width + 34)
                    Instantiator {
                        model: Call.videoDevices
                        delegate: MenuItem {
                            text: modelData
                            checked: Call.videoIndex === index
                            checkable: true
                            onTriggered: {
                                Call.videoIndex = index
                                if (!Call.cameraEnabled)
                                    Call.cameraEnabled = true
                            }
                        }
                        onObjectAdded: function(i, obj) { camMenu.insertItem(i, obj) }
                        onObjectRemoved: function(i, obj) { camMenu.removeItem(obj) }
                    }
                }
            }
        }

        // Speaker: click = mute output, ▾ = choose output device + volume
        Row {
            spacing: 0

            Button {
                id: speakerBtn
                width: 108
                text: Call.speakerEnabled ? qsTr("Speaker") : qsTr("Spk off")
                checked: Call.speakerEnabled
                checkable: true
                onClicked: Call.speakerEnabled = !Call.speakerEnabled
                background: Rectangle {
                    radius: theme.radiusSm
                    color: Call.speakerEnabled ? theme.surfaceAlt : "#3F1D1D"
                    border.color: theme.border
                }
            }
            Button {
                width: 34
                text: "▾"
                onClicked: root.openSpeakerMenu()
                background: Rectangle {
                    radius: theme.radiusSm
                    color: theme.surfaceHover
                    border.color: theme.border
                }
                Menu {
                    id: speakerMenu
                    y: -implicitHeight - 8
                    width: Math.max(280, speakerBtn.width + 34)

                    Instantiator {
                        model: Call.speakerDevices
                        delegate: MenuItem {
                            text: modelData
                            checked: Call.speakerIndex === index
                            checkable: true
                            onTriggered: Call.speakerIndex = index
                        }
                        onObjectAdded: function(i, obj) { speakerMenu.insertItem(i, obj) }
                        onObjectRemoved: function(i, obj) { speakerMenu.removeItem(obj) }
                    }
                    MenuSeparator {}
                    MenuItem {
                        enabled: false
                        text: qsTr("Speaker volume  %1%").arg(Math.round(Call.speakerVolume * 100))
                    }
                    MenuItem {
                        text: qsTr("Volume −")
                        onTriggered: Call.speakerVolume = Math.max(0, Call.speakerVolume - 0.1)
                    }
                    MenuItem {
                        text: qsTr("Volume +")
                        onTriggered: Call.speakerVolume = Math.min(1, Call.speakerVolume + 0.1)
                    }
                    MenuSeparator {}
                    MenuItem {
                        enabled: false
                        text: qsTr("Mic level  %1%").arg(Math.round(Call.micVolume * 100))
                    }
                    MenuItem {
                        text: qsTr("Mic level −")
                        onTriggered: Call.micVolume = Math.max(0, Call.micVolume - 0.1)
                    }
                    MenuItem {
                        text: qsTr("Mic level +")
                        onTriggered: Call.micVolume = Math.min(1, Call.micVolume + 0.1)
                    }
                }
            }
        }

        Button {
            visible: root.showLeave
            text: root.leaveText
            onClicked: root.leaveRequested()
            palette.buttonText: theme.danger
        }
    }
}
