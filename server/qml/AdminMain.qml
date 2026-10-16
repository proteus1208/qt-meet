import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMeet.Server
import "theme"

ApplicationWindow {
    id: window
    width: 1120
    height: 740
    minimumWidth: 960
    minimumHeight: 620
    visible: true
    title: qsTr("QtMeet Server")
    color: theme.bg

    readonly property Theme theme: Theme {}
    property int currentPage: 0 // 0 Service, 1 Library, 2 Channels, 3 Users, 4 Settings

    FolderDialog {
        id: folderDialog
        title: qsTr("Select folder to host")
        currentFolder: Admin.hostedFolder.length
                       ? ("file:///" + String(Admin.hostedFolder).replace(/\\/g, "/"))
                       : ""
        onAccepted: Admin.chooseFolder(selectedFolder)
    }

    Connections {
        target: Admin
        function onErrorOccurred(message) { toast.show(message, true) }
    }

    function userInitials(name) {
        const parts = String(name || "").trim().split(/\s+/)
        if (parts.length >= 2)
            return (parts[0][0] + parts[1][0]).toUpperCase()
        return String(name || "?").substring(0, 2).toUpperCase()
    }

    function userMatches(row, query) {
        if (!query || query.length === 0)
            return true
        const q = query.toLowerCase()
        const name = String(row.name || "").toLowerCase()
        const user = String(row.username || "").toLowerCase()
        return name.indexOf(q) >= 0 || user.indexOf(q) >= 0
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Sidebar ──────────────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: window.theme.surface
            border.color: window.theme.border
            border.width: 0

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: 9
                        color: window.theme.accent
                        Column {
                            anchors.centerIn: parent
                            spacing: 3
                            Repeater {
                                model: 3
                                Rectangle {
                                    width: 14
                                    height: 2
                                    radius: 1
                                    color: "white"
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        spacing: 0
                        Label {
                            text: qsTr("QtMeet")
                            color: window.theme.text
                            font.pixelSize: 16
                            font.bold: true
                        }
                        Label {
                            text: qsTr("Server admin")
                            color: window.theme.textDim
                            font.pixelSize: 11
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    Layout.topMargin: 8
                    radius: 14
                    color: Admin.running ? window.theme.successSoft : window.theme.surfaceAlt
                    border.color: Admin.running ? window.theme.success : window.theme.border
                    border.width: 1
                    Row {
                        anchors.centerIn: parent
                        spacing: 6
                        Rectangle {
                            width: 7
                            height: 7
                            radius: 3.5
                            anchors.verticalCenter: parent.verticalCenter
                            color: Admin.running ? window.theme.success : window.theme.textDim
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Admin.running ? qsTr("Running") : qsTr("Stopped")
                            color: Admin.running ? window.theme.success : window.theme.textMuted
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                }

                Item { Layout.preferredHeight: 8 }

                Repeater {
                    model: [
                        { title: qsTr("Service"), hint: qsTr("Start / stop") },
                        { title: qsTr("Library"), hint: qsTr("Hosted folder") },
                        { title: qsTr("Channels"), hint: qsTr("Live IPTV") },
                        { title: qsTr("Users"), hint: qsTr("Online clients") },
                        { title: qsTr("Settings"), hint: qsTr("Delivery & bitrate") }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        radius: window.theme.radiusSm
                        color: window.currentPage === index ? window.theme.surfaceHover : "transparent"
                        border.color: window.currentPage === index ? window.theme.border : "transparent"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 0
                            Label {
                                text: modelData.title
                                color: window.theme.text
                                font.pixelSize: 13
                                font.bold: window.currentPage === index
                            }
                            Label {
                                text: modelData.hint
                                color: window.theme.textDim
                                font.pixelSize: 10
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                window.currentPage = index
                                if (index === 2) {
                                    Admin.refreshSources()
                                    Admin.refreshChannels()
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: Admin.listenInfo
                    color: window.theme.textDim
                    font.pixelSize: 10
                }
            }

            Rectangle {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: 1
                color: window.theme.border
            }
        }

        // ── Content ──────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                color: window.theme.bg
                Label {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 28
                    text: [
                            qsTr("Service"),
                            qsTr("Library"),
                            qsTr("Live channels"),
                            qsTr("Online users"),
                            qsTr("Settings")
                          ][window.currentPage]
                    color: window.theme.text
                    font.pixelSize: 20
                    font.bold: true
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: window.theme.border
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 24
                currentIndex: window.currentPage

                // 0 — Service
                ColumnLayout {
                    spacing: 16

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: qsTr("Start or stop the control service. Clients connect here for chat, calls, meetings, and live channels.")
                        color: window.theme.textMuted
                        font.pixelSize: 13
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            radius: window.theme.radiusSm
                            color: Admin.running ? window.theme.surfaceHover : window.theme.success
                            opacity: Admin.running ? 0.5 : 1
                            Label {
                                anchors.centerIn: parent
                                text: qsTr("Start")
                                color: "white"
                                font.pixelSize: 15
                                font.bold: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: !Admin.running
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: {
                                    if (Admin.hostedFolder.length === 0) {
                                        toast.show(qsTr("Choose a hosted folder on the Library page first."), true)
                                        window.currentPage = 1
                                        return
                                    }
                                    Admin.start()
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            radius: window.theme.radiusSm
                            color: Admin.running ? "#B91C1C" : window.theme.stop
                            opacity: Admin.running ? 1 : 0.5
                            Label {
                                anchors.centerIn: parent
                                text: qsTr("Stop")
                                color: "white"
                                font.pixelSize: 15
                                font.bold: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: Admin.running
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: Admin.stop()
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 100
                        radius: window.theme.radius
                        color: window.theme.surface
                        border.color: window.theme.border
                        border.width: 1
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 16
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Label {
                                    text: qsTr("Active connections")
                                    color: window.theme.textMuted
                                    font.pixelSize: 12
                                }
                                Label {
                                    text: String(Admin.connectionCount)
                                    color: window.theme.text
                                    font.pixelSize: 36
                                    font.bold: true
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Label {
                                    text: qsTr("Listen")
                                    color: window.theme.textMuted
                                    font.pixelSize: 12
                                }
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    text: Admin.listenInfo
                                    color: window.theme.text
                                    font.pixelSize: 13
                                }
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: qsTr("Tip: open Channels to publish camera/screen feeds, Settings to pick Direct vs Multicast, Library to set the file folder.")
                        color: window.theme.textDim
                        font.pixelSize: 12
                    }

                    Item { Layout.fillHeight: true }
                }

                // 1 — Library
                ColumnLayout {
                    spacing: 16

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: qsTr("Clients browse and play media from this folder. Your choice is saved and restored next time you open the server.")
                        color: window.theme.textMuted
                        font.pixelSize: 13
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        radius: window.theme.radius
                        color: window.theme.surface
                        border.color: window.theme.border
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 12

                            Label {
                                text: qsTr("Hosted folder")
                                color: window.theme.text
                                font.pixelSize: 14
                                font.bold: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 40
                                    radius: window.theme.radiusSm
                                    color: window.theme.surfaceAlt
                                    border.color: window.theme.border
                                    border.width: 1
                                    Label {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideMiddle
                                        text: Admin.hostedFolder.length
                                              ? Admin.hostedFolder
                                              : qsTr("No folder selected")
                                        color: Admin.hostedFolder.length
                                               ? window.theme.text
                                               : window.theme.textDim
                                        font.pixelSize: 13
                                    }
                                }

                                Rectangle {
                                    Layout.preferredWidth: 120
                                    Layout.preferredHeight: 40
                                    radius: window.theme.radiusSm
                                    color: window.theme.accent
                                    Label {
                                        anchors.centerIn: parent
                                        text: qsTr("Browse…")
                                        color: "white"
                                        font.bold: true
                                        font.pixelSize: 13
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: folderDialog.open()
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }

                // 2 — Channels
                ColumnLayout {
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: qsTr("Create a channel with video only, audio only, or both. System output appears in the audio list.")
                        color: window.theme.textMuted
                        font.pixelSize: 13
                    }

                    TextField {
                        id: channelName
                        Layout.fillWidth: true
                        placeholderText: qsTr("Channel name")
                        color: window.theme.text
                        enabled: Admin.running
                        background: Rectangle {
                            radius: window.theme.radiusSm
                            color: window.theme.surfaceAlt
                            border.color: window.theme.border
                            border.width: 1
                        }
                    }

                    Label {
                        text: qsTr("Video source")
                        color: window.theme.textMuted
                        font.pixelSize: 11
                    }
                    ComboBox {
                        id: videoSourceBox
                        Layout.fillWidth: true
                        model: Admin.videoSourceLabels
                        enabled: Admin.running
                    }
                    Label {
                        text: qsTr("Audio source (mic or system output)")
                        color: window.theme.textMuted
                        font.pixelSize: 11
                    }
                    ComboBox {
                        id: audioSourceBox
                        Layout.fillWidth: true
                        model: Admin.audioSourceLabels
                        enabled: Admin.running
                    }

                    Label {
                        text: qsTr("Channel bitrate: %1 kbps").arg(Math.round(newChannelBitrate.value))
                        color: window.theme.textMuted
                        font.pixelSize: 11
                    }
                    Slider {
                        id: newChannelBitrate
                        Layout.fillWidth: true
                        from: 200
                        to: 4000
                        stepSize: 50
                        value: Admin.defaultChannelBitrateKbps
                        enabled: Admin.running
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Button {
                            text: qsTr("Refresh devices")
                            onClicked: Admin.refreshSources()
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: qsTr("Add channel")
                            enabled: Admin.running && channelName.text.trim().length > 0
                                     && (videoSourceBox.currentIndex > 0 || audioSourceBox.currentIndex > 0)
                            onClicked: {
                                if (Admin.addChannel(channelName.text.trim(),
                                                     videoSourceBox.currentIndex,
                                                     audioSourceBox.currentIndex,
                                                     Math.round(newChannelBitrate.value)))
                                    channelName.text = ""
                            }
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 8
                        model: Admin.channels

                        delegate: Rectangle {
                            id: chDelegate
                            required property var modelData
                            width: ListView.view.width
                            height: chCol.implicitHeight + 20
                            radius: window.theme.radiusSm
                            color: window.theme.surface
                            border.color: window.theme.border
                            border.width: 1

                            ColumnLayout {
                                id: chCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 12
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true
                                        text: chDelegate.modelData.name || ""
                                        color: window.theme.text
                                        font.bold: true
                                        font.pixelSize: 14
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: chDelegate.modelData.running ? qsTr("LIVE") : qsTr("Off")
                                        color: chDelegate.modelData.running
                                               ? window.theme.success : window.theme.textDim
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Video: %1").arg(chDelegate.modelData.videoLabel || "—")
                                    color: window.theme.textMuted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Audio: %1 · Viewers %2")
                                          .arg(chDelegate.modelData.audioLabel || "—")
                                          .arg(chDelegate.modelData.viewers || 0)
                                    color: window.theme.textMuted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Bitrate: %1 kbps").arg(chDelegate.modelData.bitrateKbps || Admin.defaultChannelBitrateKbps)
                                    color: window.theme.textMuted
                                    font.pixelSize: 11
                                }
                                Slider {
                                    Layout.fillWidth: true
                                    from: 200
                                    to: 4000
                                    stepSize: 50
                                    value: chDelegate.modelData.bitrateKbps || Admin.defaultChannelBitrateKbps
                                    enabled: Admin.running
                                    onMoved: Admin.setChannelBitrate(chDelegate.modelData.id, Math.round(value))
                                }
                                RowLayout {
                                    spacing: 8
                                    Button {
                                        text: chDelegate.modelData.running ? qsTr("Stop") : qsTr("Start")
                                        enabled: Admin.running
                                        onClicked: {
                                            if (chDelegate.modelData.running)
                                                Admin.stopChannel(chDelegate.modelData.id)
                                            else
                                                Admin.startChannel(chDelegate.modelData.id)
                                        }
                                    }
                                    Button {
                                        text: qsTr("Remove")
                                        enabled: Admin.running
                                        onClicked: Admin.removeChannel(chDelegate.modelData.id)
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        visible: Admin.channels.length === 0
                        text: Admin.running
                              ? qsTr("No channels yet — add one above.")
                              : qsTr("Start the service first, then add live channels.")
                        color: window.theme.textDim
                        font.pixelSize: 12
                    }
                }

                // 3 — Users
                ColumnLayout {
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: qsTr("Registered accounts. Online users are highlighted.")
                        color: window.theme.textMuted
                        font.pixelSize: 13
                    }

                    TextField {
                        id: userSearch
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search by name or username…")
                        color: window.theme.text
                        background: Rectangle {
                            radius: window.theme.radiusSm
                            color: window.theme.surfaceAlt
                            border.color: window.theme.border
                            border.width: 1
                        }
                    }

                    ListView {
                        id: usersList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: Admin.onlineUsers

                        delegate: Rectangle {
                            id: userDelegate
                            required property var modelData
                            required property int index
                            readonly property string displayName: String(modelData.name || "")
                            readonly property string userName: String(modelData.username || "")
                            readonly property bool isOnline: !!modelData.online
                            readonly property var palette: ["#3B82F6", "#14B8A6", "#8B5CF6", "#F59E0B", "#EF4444"]
                            width: ListView.view.width
                            height: window.userMatches(modelData, userSearch.text) ? 56 : 0
                            visible: height > 0
                            radius: window.theme.radiusSm
                            color: window.theme.surface
                            border.color: window.theme.border
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 12
                                Rectangle {
                                    Layout.preferredWidth: 36
                                    Layout.preferredHeight: 36
                                    radius: 18
                                    color: userDelegate.palette[userDelegate.index % userDelegate.palette.length]
                                    Text {
                                        anchors.centerIn: parent
                                        text: window.userInitials(userDelegate.displayName)
                                        color: "white"
                                        font.bold: true
                                        font.pixelSize: 12
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        text: userDelegate.displayName
                                        color: window.theme.text
                                        font.pixelSize: 14
                                        font.bold: true
                                    }
                                    Label {
                                        text: userDelegate.userName.length
                                              ? ("@" + userDelegate.userName)
                                              : qsTr("Registered user")
                                        color: window.theme.textMuted
                                        font.pixelSize: 11
                                    }
                                }
                                Rectangle {
                                    Layout.preferredHeight: 22
                                    Layout.preferredWidth: statusLabel.implicitWidth + 16
                                    radius: 11
                                    color: userDelegate.isOnline ? window.theme.successSoft : window.theme.surfaceAlt
                                    border.color: userDelegate.isOnline ? window.theme.success : window.theme.border
                                    border.width: 1
                                    Label {
                                        id: statusLabel
                                        anchors.centerIn: parent
                                        text: userDelegate.isOnline ? qsTr("Online") : qsTr("Offline")
                                        color: userDelegate.isOnline ? window.theme.success : window.theme.textDim
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        visible: Admin.onlineUsers.length === 0
                        text: Admin.running
                              ? qsTr("No registered users yet.")
                              : qsTr("Start the service to load the user directory.")
                        color: window.theme.textDim
                        font.pixelSize: 12
                    }
                }

                // 4 — Settings
                ColumnLayout {
                    spacing: 18

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: qsTr("Delivery and default bitrates. Saved automatically and sent to clients on login.")
                        color: window.theme.textMuted
                        font.pixelSize: 13
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: settingsCol.implicitHeight + 36
                        radius: window.theme.radius
                        color: window.theme.surface
                        border.color: window.theme.border
                        border.width: 1

                        ColumnLayout {
                            id: settingsCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 18
                            spacing: 14

                            Label {
                                text: qsTr("Delivery mode")
                                color: window.theme.text
                                font.pixelSize: 14
                                font.bold: true
                            }
                            ComboBox {
                                Layout.fillWidth: true
                                model: Admin.transportModeLabels
                                currentIndex: Admin.transportMode
                                onActivated: Admin.transportMode = index
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: Admin.transportMode === 1
                                      ? qsTr("Multicast: one shared encode for channels, best with many viewers.")
                                      : qsTr("Direct: unicast SFU relay to each viewer and call participant.")
                                color: window.theme.textMuted
                                font.pixelSize: 12
                            }

                            Label {
                                Layout.topMargin: 8
                                text: qsTr("Default channel bitrate")
                                color: window.theme.text
                                font.pixelSize: 14
                                font.bold: true
                            }
                            Label {
                                text: qsTr("%1 kbps").arg(Admin.defaultChannelBitrateKbps)
                                color: window.theme.textMuted
                                font.pixelSize: 12
                            }
                            Slider {
                                Layout.fillWidth: true
                                from: 200
                                to: 4000
                                stepSize: 50
                                value: Admin.defaultChannelBitrateKbps
                                onMoved: Admin.defaultChannelBitrateKbps = Math.round(value)
                            }

                            Label {
                                Layout.topMargin: 8
                                text: qsTr("1:1 call bitrate")
                                color: window.theme.text
                                font.pixelSize: 14
                                font.bold: true
                            }
                            Label {
                                text: qsTr("%1 kbps").arg(Admin.defaultCallBitrateKbps)
                                color: window.theme.textMuted
                                font.pixelSize: 12
                            }
                            Slider {
                                Layout.fillWidth: true
                                from: 200
                                to: 4000
                                stepSize: 50
                                value: Admin.defaultCallBitrateKbps
                                onMoved: Admin.defaultCallBitrateKbps = Math.round(value)
                            }

                            Label {
                                Layout.topMargin: 8
                                text: qsTr("Meeting bitrate")
                                color: window.theme.text
                                font.pixelSize: 14
                                font.bold: true
                            }
                            Label {
                                text: qsTr("%1 kbps").arg(Admin.defaultMeetingBitrateKbps)
                                color: window.theme.textMuted
                                font.pixelSize: 12
                            }
                            Slider {
                                Layout.fillWidth: true
                                from: 200
                                to: 4000
                                stepSize: 50
                                value: Admin.defaultMeetingBitrateKbps
                                onMoved: Admin.defaultMeetingBitrateKbps = Math.round(value)
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    Rectangle {
        id: toast
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 24
        width: Math.min(toastLabel.implicitWidth + 72, parent.width - 48)
        height: 48
        radius: 10
        color: window.theme.dangerSoft
        border.color: window.theme.danger
        border.width: 1
        visible: opacity > 0
        opacity: 0
        z: 100

        function show(message, isError) {
            toastLabel.text = message
            color = isError ? window.theme.dangerSoft : window.theme.successSoft
            border.color = isError ? window.theme.danger : window.theme.success
            opacity = 1
            hideTimer.restart()
        }

        Label {
            id: toastLabel
            anchors.fill: parent
            anchors.margins: 14
            color: window.theme.text
            font.pixelSize: 13
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Timer {
            id: hideTimer
            interval: 4500
            onTriggered: toast.opacity = 0
        }
        Behavior on opacity { NumberAnimation { duration: 180 } }
    }
}
