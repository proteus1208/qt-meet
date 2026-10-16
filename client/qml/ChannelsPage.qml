import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet
import "theme"

Page {
    id: page
    title: qsTr("Live channels")

    readonly property Theme theme: Theme {}

    Component.onCompleted: Call.refreshChannels()

    Connections {
        target: Call
        function onStateChanged() {
            if (Call.state === 5) // Watching
                page.StackView.view.push(Qt.resolvedUrl("ChannelWatchView.qml"))
        }
        function onErrorOccurred(message) {
            errorLabel.text = message
        }
    }

    header: ToolBar {
        background: Rectangle { color: theme.surface }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            Button {
                text: qsTr("Back")
                flat: true
                onClicked: page.StackView.view.pop()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Live channels")
                color: theme.text
                font.bold: true
                font.pixelSize: 16
            }

            Button {
                text: qsTr("Refresh")
                enabled: Session.loggedIn
                onClicked: Call.refreshChannels()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: theme.bg
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: theme.textMuted
            font.pixelSize: 13
            text: qsTr("Watch live camera or screen channels published by the server. Delivery: %1.")
                  .arg(Session.transportModeLabel)
        }

        Label {
            id: errorLabel
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "#FBBF24"
            font.pixelSize: 12
            visible: text.length > 0
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 12
            model: Call.channels

            delegate: Rectangle {
                id: row
                required property var modelData
                width: ListView.view.width
                height: col.implicitHeight + 28
                radius: theme.radius
                color: theme.surface
                border.color: row.modelData.running ? theme.success : theme.border
                border.width: 1
                opacity: row.modelData.running ? 1.0 : 0.7

                ColumnLayout {
                    id: col
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.name || qsTr("Channel")
                            color: theme.text
                            font.pixelSize: 22
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            width: liveTxt.implicitWidth + 16
                            height: 26
                            radius: 13
                            color: row.modelData.running ? "#14532D" : theme.surfaceHover
                            border.color: row.modelData.running ? theme.success : theme.border
                            border.width: 1
                            Label {
                                id: liveTxt
                                anchors.centerIn: parent
                                text: row.modelData.running ? qsTr("LIVE") : qsTr("Offline")
                                color: row.modelData.running ? theme.success : theme.textDim
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        Label {
                            text: qsTr("%1 visitors").arg(row.modelData.viewers || 0)
                            color: theme.text
                            font.pixelSize: 14
                            font.bold: true
                        }
                        Label {
                            text: "·"
                            color: theme.textDim
                        }
                        Label {
                            text: qsTr("%1 kbps").arg(row.modelData.bitrateKbps || "—")
                            color: theme.text
                            font.pixelSize: 14
                            font.bold: true
                        }
                        Label {
                            text: "·"
                            color: theme.textDim
                        }
                        Label {
                            Layout.fillWidth: true
                            text: {
                                const v = row.modelData.videoLabel || "—"
                                const a = row.modelData.audioLabel || "—"
                                return qsTr("Video %1 · Audio %2").arg(v).arg(a)
                            }
                            color: theme.textMuted
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }

                    Button {
                        text: qsTr("Watch")
                        enabled: Session.loggedIn && Call.state === 0 && row.modelData.running
                        onClicked: Call.joinChannel(row.modelData.id)
                    }
                }
            }
        }

        Label {
            visible: Call.channels.length === 0
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("No live channels yet. Ask the server admin to add one.")
            color: theme.textDim
            font.pixelSize: 13
        }
    }
}
