import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMeet.Server
import "theme"

ApplicationWindow {
    id: window
    width: 980
    height: 680
    minimumWidth: 820
    minimumHeight: 560
    visible: true
    title: qsTr("QtMeet Server")
    color: theme.bg

    readonly property Theme theme: Theme {}

    FolderDialog {
        id: folderDialog
        title: qsTr("Select folder to host")
        onAccepted: Admin.chooseFolder(selectedFolder)
    }

    Connections {
        target: Admin
        function onErrorOccurred(message) {
            toast.show(message, true)
        }
    }

    ListModel {
        id: userModel

        function rebuild() {
            clear()
            const live = Admin.onlineUsers
            if (live.length > 0) {
                for (let i = 0; i < live.length; ++i) {
                    const name = String(live[i])
                    const parts = name.trim().split(/\s+/)
                    let initials = name.substring(0, 2).toUpperCase()
                    if (parts.length >= 2)
                        initials = (parts[0][0] + parts[1][0]).toUpperCase()
                    const palette = ["#3B82F6", "#14B8A6", "#8B5CF6", "#F59E0B", "#EF4444"]
                    append({
                               name: name,
                               initials: initials,
                               avatarColor: palette[i % palette.length],
                               ago: qsTr("now"),
                               isPreview: false
                           })
                }
            } else {
                append({ name: "John Doe", initials: "JD", avatarColor: "#3B82F6", ago: "2m ago", isPreview: true })
                append({ name: "Alice Smith", initials: "AS", avatarColor: "#14B8A6", ago: "5m ago", isPreview: true })
                append({ name: "Mark Kim", initials: "MK", avatarColor: "#8B5CF6", ago: "15m ago", isPreview: true })
            }
        }

        Component.onCompleted: rebuild()
    }

    Connections {
        target: Admin
        function onStatsChanged() { userModel.rebuild() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Rectangle {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: 10
                color: window.theme.accent

                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    Repeater {
                        model: 3
                        Rectangle {
                            width: 16
                            height: 2
                            radius: 1
                            color: "white"
                        }
                    }
                }
            }

            Label {
                text: qsTr("QtMeet Server")
                color: window.theme.text
                font.pixelSize: 22
                font.bold: true
            }

            Rectangle {
                Layout.preferredWidth: statusRow.implicitWidth + 18
                Layout.preferredHeight: 28
                radius: 14
                color: Admin.running ? window.theme.successSoft : window.theme.surfaceHover
                border.color: Admin.running ? window.theme.success : window.theme.border
                border.width: 1

                Row {
                    id: statusRow
                    anchors.centerIn: parent
                    spacing: 6

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 8
                        height: 8
                        radius: 4
                        color: Admin.running ? window.theme.success : window.theme.textDim
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Admin.running ? qsTr("Running") : qsTr("Stopped")
                        color: Admin.running ? window.theme.success : window.theme.textMuted
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            Item { Layout.fillWidth: true }

            RowLayout {
                spacing: 8
                Text {
                    text: "📶"
                    font.pixelSize: 16
                }
                Label {
                    text: Admin.listenInfo.length > 0 ? Admin.listenInfo : qsTr("Not listening")
                    color: window.theme.textMuted
                    font.pixelSize: 13
                }
            }
        }

        // Body
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18

            // Left column
            ColumnLayout {
                Layout.preferredWidth: parent.width * 0.42
                Layout.fillHeight: true
                spacing: 16

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: folderCol.implicitHeight + 36
                    radius: window.theme.radius
                    color: window.theme.surface
                    border.color: window.theme.border
                    border.width: 1

                    ColumnLayout {
                        id: folderCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 18
                        spacing: 12

                        RowLayout {
                            spacing: 8
                            Text { text: "📁"; font.pixelSize: 16 }
                            Label {
                                text: qsTr("Hosted folder")
                                color: window.theme.text
                                font.pixelSize: 16
                                font.bold: true
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Clients can browse and play files from this folder.")
                            color: window.theme.textMuted
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
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
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    verticalAlignment: Text.AlignVCenter
                                    text: Admin.hostedFolder.length > 0
                                          ? Admin.hostedFolder
                                          : qsTr("No folder selected")
                                    color: Admin.hostedFolder.length > 0
                                           ? window.theme.text
                                           : window.theme.textDim
                                    font.pixelSize: 13
                                    elide: Text.ElideMiddle
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: browseRow.implicitWidth + 36
                                Layout.preferredHeight: 40
                                radius: window.theme.radiusSm
                                color: window.theme.accent

                                Row {
                                    id: browseRow
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Text {
                                        text: "📂"
                                        font.pixelSize: 13
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label {
                                        text: qsTr("Browse...")
                                        color: "white"
                                        font.pixelSize: 13
                                        font.bold: true
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
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

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: window.theme.radius
                    color: window.theme.surface
                    border.color: window.theme.border
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 14

                        RowLayout {
                            spacing: 8
                            Text { text: "⚙"; font.pixelSize: 16 }
                            Label {
                                text: qsTr("Service")
                                color: window.theme.text
                                font.pixelSize: 16
                                font.bold: true
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Start or stop the server and manage connections.")
                            color: window.theme.textMuted
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 48
                                radius: window.theme.radiusSm
                                color: Admin.running ? window.theme.surfaceHover : window.theme.success
                                opacity: Admin.running ? 0.55 : 1.0

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 8
                                    Text {
                                        text: "▶"
                                        color: "white"
                                        font.pixelSize: 14
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label {
                                        text: qsTr("Start")
                                        color: "white"
                                        font.pixelSize: 15
                                        font.bold: true
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: !Admin.running
                                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: {
                                        if (Admin.hostedFolder.length === 0) {
                                            toast.show(qsTr("Failed to start server: No hosted folder selected."), true)
                                            return
                                        }
                                        Admin.start()
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 48
                                radius: window.theme.radiusSm
                                color: Admin.running ? "#B91C1C" : window.theme.stop
                                opacity: Admin.running ? 1.0 : 0.55

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 8
                                    Text {
                                        text: "■"
                                        color: "white"
                                        font.pixelSize: 12
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label {
                                        text: qsTr("Stop")
                                        color: "white"
                                        font.pixelSize: 15
                                        font.bold: true
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
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
                            Layout.preferredHeight: 72
                            radius: window.theme.radiusSm
                            color: window.theme.surfaceAlt
                            border.color: window.theme.border
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 12

                                Text { text: "👥"; font.pixelSize: 22 }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        text: qsTr("Active connections")
                                        color: window.theme.textMuted
                                        font.pixelSize: 12
                                    }
                                    Label {
                                        text: String(Admin.connectionCount)
                                        color: window.theme.text
                                        font.pixelSize: 28
                                        font.bold: true
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }

            // Online users
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: window.theme.radius
                color: window.theme.surface
                border.color: window.theme.border
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text { text: "👥"; font.pixelSize: 16 }
                        Label {
                            text: qsTr("Online users")
                            color: window.theme.text
                            font.pixelSize: 16
                            font.bold: true
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            Layout.preferredWidth: 200
                            Layout.preferredHeight: 36
                            radius: 18
                            color: window.theme.surfaceAlt
                            border.color: window.theme.border
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 6

                                Text {
                                    text: "⌕"
                                    color: window.theme.textDim
                                    font.pixelSize: 14
                                }

                                TextInput {
                                    id: userSearch
                                    Layout.fillWidth: true
                                    color: window.theme.text
                                    font.pixelSize: 12
                                    clip: true
                                    Text {
                                        anchors.fill: parent
                                        verticalAlignment: Text.AlignVCenter
                                        text: qsTr("Search users...")
                                        color: window.theme.textDim
                                        font.pixelSize: 12
                                        visible: !userSearch.text && !userSearch.activeFocus
                                    }
                                }
                            }
                        }
                    }

                    ListView {
                        id: userList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: userModel

                        delegate: Rectangle {
                            id: userDelegate
                            required property int index
                            required property string name
                            required property string initials
                            required property string avatarColor
                            required property string ago

                            width: userList.width
                            height: (userSearch.text.length === 0
                                     || name.toLowerCase().indexOf(userSearch.text.toLowerCase()) >= 0)
                                    ? 56 : 0
                            radius: window.theme.radiusSm
                            color: hover.hovered ? window.theme.surfaceHover : "transparent"
                            visible: height > 0
                            clip: true

                            HoverHandler { id: hover }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 12
                                visible: userDelegate.visible

                                Rectangle {
                                    Layout.preferredWidth: 40
                                    Layout.preferredHeight: 40
                                    radius: 20
                                    color: userDelegate.avatarColor

                                    Text {
                                        anchors.centerIn: parent
                                        text: userDelegate.initials
                                        color: "white"
                                        font.bold: true
                                        font.pixelSize: 13
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: userDelegate.name
                                        color: window.theme.text
                                        font.pixelSize: 14
                                        font.bold: true
                                    }

                                    RowLayout {
                                        spacing: 6
                                        Rectangle {
                                            width: 7
                                            height: 7
                                            radius: 3.5
                                            color: window.theme.success
                                        }
                                        Label {
                                            text: qsTr("Online")
                                            color: window.theme.textMuted
                                            font.pixelSize: 11
                                        }
                                    }
                                }

                                Label {
                                    text: userDelegate.ago
                                    color: window.theme.textDim
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        radius: window.theme.radiusSm
                        color: window.theme.surfaceAlt
                        border.color: window.theme.border
                        border.width: 1
                        visible: Admin.onlineUsers.length === 0

                        Column {
                            anchors.centerIn: parent
                            spacing: 4
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "👤"
                                font.pixelSize: 16
                                opacity: 0.35
                            }
                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: qsTr("No users online — preview list shown above")
                                color: window.theme.textDim
                                font.pixelSize: 11
                            }
                        }
                    }
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

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 12
            spacing: 10

            Text {
                text: "✕"
                color: window.theme.danger
                font.pixelSize: 14
                font.bold: true
            }

            Label {
                id: toastLabel
                Layout.fillWidth: true
                color: window.theme.text
                font.pixelSize: 13
                elide: Text.ElideRight
            }

            Text {
                text: "✕"
                color: window.theme.textMuted
                font.pixelSize: 12
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    cursorShape: Qt.PointingHandCursor
                    onClicked: toast.opacity = 0
                }
            }
        }

        Timer {
            id: hideTimer
            interval: 4500
            onTriggered: toast.opacity = 0
        }

        Behavior on opacity { NumberAnimation { duration: 180 } }
    }
}
