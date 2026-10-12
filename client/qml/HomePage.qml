import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet
import "theme"

Page {
    id: page

    readonly property Theme theme: Theme {}
    property int currentNav: 0 // 0 chats, 1 people, 2 meetings, 3 files
    property int selectedChat: 0
    // True once a real user is picked from People; the demo chat pane is used otherwise.
    readonly property bool peerMode: Chat.activePeerId !== 0

    background: Rectangle { color: page.theme.bg }

    Component.onCompleted: Chat.searchUsers("")

    function initialsOf(name) {
        if (!name || name.length === 0)
            return "?"
        const parts = name.trim().split(" ").filter(p => p.length > 0)
        if (parts.length >= 2)
            return (parts[0][0] + parts[1][0]).toUpperCase()
        return name.trim().substring(0, 2).toUpperCase()
    }

    readonly property var avatarPalette: ["#3B82F6", "#8B5CF6", "#F59E0B", "#10B981", "#EF4444", "#06B6D4", "#EC4899"]
    function avatarColorFor(userId) {
        return avatarPalette[Number(userId) % avatarPalette.length]
    }

    // Values match proto::Presence: 0 offline, 1 online, 2 busy, 3 in call.
    function presenceColor(presence) {
        switch (presence) {
        case 1: return page.theme.success
        case 2: return "#F59E0B"
        case 3: return page.theme.danger
        default: return page.theme.textDim
        }
    }
    function presenceText(presence) {
        switch (presence) {
        case 1: return qsTr("Online")
        case 2: return qsTr("Busy")
        case 3: return qsTr("In call")
        default: return qsTr("Offline")
        }
    }

    // Demo data until controllers feed real models
    ListModel {
        id: chatModel
        ListElement {
            name: "Alice Smith"
            initials: "AS"
            avatarColor: "#3B82F6"
            preview: "Sure, I'll send the design files..."
            time: "10:24"
            unread: 2
            online: true
        }
        ListElement {
            name: "Mark Kim"
            initials: "MK"
            avatarColor: "#8B5CF6"
            preview: "Meeting notes attached."
            time: "Yesterday"
            unread: 0
            online: true
        }
        ListElement {
            name: "Design Team"
            initials: "DT"
            avatarColor: "#F59E0B"
            preview: "New mockups are ready for review"
            time: "Mon"
            unread: 5
            online: false
        }
        ListElement {
            name: "Sarah Chen"
            initials: "SC"
            avatarColor: "#10B981"
            preview: "Can we reschedule to 3pm?"
            time: "Mon"
            unread: 0
            online: false
        }
        ListElement {
            name: "Project Alpha"
            initials: "PA"
            avatarColor: "#EF4444"
            preview: "Sprint planning tomorrow"
            time: "Sun"
            unread: 1
            online: false
        }
    }

    ListModel {
        id: messageModel
        ListElement {
            fromMe: false
            sender: "Alice Smith"
            initials: "AS"
            avatarColor: "#3B82F6"
            text: "Hey! Are you free for a quick call about the project?"
            time: "10:18"
            isFile: false
            fileName: ""
            fileSize: ""
        }
        ListElement {
            fromMe: true
            sender: "You"
            initials: "JD"
            avatarColor: "#2563EB"
            text: "Sure, give me 5 minutes to wrap this up."
            time: "10:19"
            isFile: false
            fileName: ""
            fileSize: ""
        }
        ListElement {
            fromMe: false
            sender: "Alice Smith"
            initials: "AS"
            avatarColor: "#3B82F6"
            text: "Perfect. I'll also share the latest design package."
            time: "10:20"
            isFile: false
            fileName: ""
            fileSize: ""
        }
        ListElement {
            fromMe: false
            sender: "Alice Smith"
            initials: "AS"
            avatarColor: "#3B82F6"
            text: ""
            time: "10:21"
            isFile: true
            fileName: "Project_Design.zip"
            fileSize: "12.4 MB"
        }
        ListElement {
            fromMe: true
            sender: "You"
            initials: "JD"
            avatarColor: "#2563EB"
            text: "Got it — downloading now. Ready when you are."
            time: "10:22"
            isFile: false
            fileName: ""
            fileSize: ""
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── 1. Nav rail ──────────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 72
            Layout.fillHeight: true
            color: page.theme.surface

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 16
                anchors.bottomMargin: 16
                spacing: 8

                // Logo
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 40
                    height: 40
                    radius: 10
                    color: page.theme.accent

                    Text {
                        anchors.centerIn: parent
                        text: "✦"
                        color: "white"
                        font.pixelSize: 20
                    }
                }

                Item { Layout.preferredHeight: 12 }

                Repeater {
                    model: [
                        { icon: "💬", label: qsTr("Chats") },
                        { icon: "👥", label: qsTr("People") },
                        { icon: "📹", label: qsTr("Meetings") },
                        { icon: "📁", label: qsTr("Files") }
                    ]

                    delegate: Item {
                        required property var modelData
                        required property int index
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 52
                        Layout.preferredHeight: 52

                        Rectangle {
                            anchors.centerIn: parent
                            width: 48
                            height: 48
                            radius: 14
                            color: page.currentNav === index ? page.theme.accent : "transparent"

                            Column {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.icon
                                    font.pixelSize: 16
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.label
                                    color: page.currentNav === index ? "white" : page.theme.textMuted
                                    font.pixelSize: 9
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    page.currentNav = index
                                    if (index === 2)
                                        page.StackView.view.push(Qt.resolvedUrl("MeetingView.qml"))
                                    else if (index === 3)
                                        page.StackView.view.push(Qt.resolvedUrl("ExplorerPage.qml"))
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // Profile
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 40
                    height: 40
                    radius: 20
                    color: page.theme.accentSoft
                    border.color: page.theme.accent
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: page.initialsOf(Session.displayName)
                        color: page.theme.accent
                        font.bold: true
                        font.pixelSize: 13
                    }
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 56
                    flat: true
                    text: qsTr("Sign out")
                    contentItem: Text {
                        text: parent.text
                        color: page.theme.textDim
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                    background: Item {}
                    onClicked: {
                        Session.logout()
                        page.StackView.view.replace(Qt.resolvedUrl("LoginPage.qml"))
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: page.theme.border
        }

        // ── 2. Chat list ─────────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 280
            Layout.fillHeight: true
            color: page.theme.surface

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Label {
                    text: page.currentNav === 1 ? qsTr("People") : qsTr("Chats")
                    color: page.theme.text
                    font.pixelSize: 22
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: 20
                    color: page.theme.surfaceAlt
                    border.color: page.theme.border
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            text: "⌕"
                            color: page.theme.textDim
                            font.pixelSize: 16
                        }

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            color: page.theme.text
                            font.pixelSize: 13
                            clip: true
                            onTextChanged: {
                                Chat.searchUsers(text)
                                if (text.length > 0)
                                    page.currentNav = 1
                            }
                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                text: qsTr("Search people...")
                                color: page.theme.textDim
                                font.pixelSize: 13
                                visible: !searchInput.text && !searchInput.activeFocus
                            }
                        }
                    }
                }

                ListView {
                    id: peopleList
                    visible: page.currentNav === 1
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: Chat.searchResults

                    delegate: Rectangle {
                        id: personDelegate
                        required property var model

                        width: peopleList.width
                        height: 60
                        radius: page.theme.radiusSm
                        color: page.peerMode && Chat.activePeerId === model.userId
                               ? page.theme.surfaceHover
                               : (personHover.hovered ? page.theme.surfaceAlt : "transparent")

                        HoverHandler { id: personHover }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                radius: 20
                                color: page.avatarColorFor(personDelegate.model.userId)

                                Text {
                                    anchors.centerIn: parent
                                    text: page.initialsOf(personDelegate.model.displayName)
                                    color: "white"
                                    font.bold: true
                                    font.pixelSize: 13
                                }

                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    width: 11
                                    height: 11
                                    radius: 5.5
                                    color: page.presenceColor(personDelegate.model.presence)
                                    border.color: page.theme.surface
                                    border.width: 2
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    text: personDelegate.model.displayName
                                    color: page.theme.text
                                    font.pixelSize: 14
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "@" + personDelegate.model.username + "  ·  "
                                          + page.presenceText(personDelegate.model.presence)
                                    color: page.theme.textMuted
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Chat.openConversationWith(personDelegate.model.userId,
                                                                 personDelegate.model.displayName)
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: peopleList.count === 0
                        text: searchInput.text.length > 0 ? qsTr("No users found") : qsTr("No other users yet")
                        color: page.theme.textDim
                        font.pixelSize: 13
                    }
                }

                ListView {
                    id: chatList
                    visible: page.currentNav !== 1
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: chatModel
                    currentIndex: page.selectedChat

                    delegate: Rectangle {
                        id: chatDelegate
                        required property int index
                        required property string name
                        required property string initials
                        required property string avatarColor
                        required property string preview
                        required property string time
                        required property int unread
                        required property bool online

                        width: chatList.width
                        height: 68
                        radius: page.theme.radiusSm
                        color: !page.peerMode && chatList.currentIndex === index ? page.theme.surfaceHover : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 44
                                Layout.preferredHeight: 44
                                radius: 22
                                color: chatDelegate.avatarColor

                                Text {
                                    anchors.centerIn: parent
                                    text: initials
                                    color: "white"
                                    font.bold: true
                                    font.pixelSize: 13
                                }

                                Rectangle {
                                    visible: online
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: page.theme.success
                                    border.color: page.theme.surface
                                    border.width: 2
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true
                                        text: name
                                        color: page.theme.text
                                        font.pixelSize: 14
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: time
                                        color: page.theme.textDim
                                        font.pixelSize: 11
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true
                                        text: preview
                                        color: page.theme.textMuted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Rectangle {
                                        visible: unread > 0
                                        Layout.preferredWidth: Math.max(18, badgeText.implicitWidth + 8)
                                        Layout.preferredHeight: 18
                                        radius: 9
                                        color: page.theme.accent
                                        Text {
                                            id: badgeText
                                            anchors.centerIn: parent
                                            text: unread
                                            color: "white"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                page.selectedChat = index
                                Chat.openConversationWith(0, "")
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: page.theme.border
        }

        // ── 3. Chat conversation ─────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: page.theme.bg

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    color: page.theme.surface

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 16
                        spacing: 12

                        ColumnLayout {
                            spacing: 2
                            Label {
                                text: page.peerMode ? Chat.activePeerName
                                                    : chatModel.get(page.selectedChat).name
                                color: page.theme.text
                                font.pixelSize: 16
                                font.bold: true
                            }
                            RowLayout {
                                spacing: 6
                                readonly property int presence: page.peerMode
                                    ? Chat.activePeerPresence
                                    : (chatModel.get(page.selectedChat).online ? 1 : 0)
                                Rectangle {
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: page.presenceColor(parent.presence)
                                }
                                Label {
                                    text: page.presenceText(parent.presence)
                                    color: page.theme.textMuted
                                    font.pixelSize: 12
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Repeater {
                            model: [
                                { icon: "📞", tip: qsTr("Voice call") },
                                { icon: "📷", tip: qsTr("Video call") },
                                { icon: "⋮", tip: qsTr("More") }
                            ]
                            delegate: Rectangle {
                                required property var modelData
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                radius: 18
                                color: hover.hovered ? page.theme.surfaceHover : "transparent"
                                border.color: page.theme.border
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.icon
                                    color: page.theme.text
                                    font.pixelSize: 14
                                }

                                HoverHandler { id: hover }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (modelData.tip.indexOf("call") >= 0 || modelData.tip.indexOf("Call") >= 0)
                                            page.StackView.view.push(Qt.resolvedUrl("CallView.qml"))
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: page.theme.border
                }

                // Messages
                ListView {
                    id: messageList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 12
                    model: page.peerMode ? null : messageModel
                    topMargin: 16
                    bottomMargin: 16
                    leftMargin: 20
                    rightMargin: 20

                    header: Item {
                        width: messageList.width - 40
                        height: 36
                        Rectangle {
                            anchors.centerIn: parent
                            width: todayLabel.implicitWidth + 24
                            height: 24
                            radius: 12
                            color: page.theme.surfaceAlt
                            Label {
                                id: todayLabel
                                anchors.centerIn: parent
                                text: qsTr("Today")
                                color: page.theme.textMuted
                                font.pixelSize: 11
                            }
                        }
                    }

                    delegate: Column {
                        id: msgDelegate
                        required property bool fromMe
                        required property string sender
                        required property string initials
                        required property string avatarColor
                        required property string text
                        required property string time
                        required property bool isFile
                        required property string fileName
                        required property string fileSize

                        width: messageList.width - 40
                        spacing: 4

                        Row {
                            anchors.right: fromMe ? parent.right : undefined
                            anchors.left: fromMe ? undefined : parent.left
                            spacing: 8
                            layoutDirection: fromMe ? Qt.RightToLeft : Qt.LeftToRight

                            Rectangle {
                                visible: !fromMe
                                width: 32
                                height: 32
                                radius: 16
                                color: msgDelegate.avatarColor
                                anchors.bottom: bubble.bottom
                                Text {
                                    anchors.centerIn: parent
                                    text: initials
                                    color: "white"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            Rectangle {
                                id: bubble
                                width: Math.min(isFile ? 280 : msgCol.implicitWidth + 28,
                                                messageList.width * 0.55)
                                height: msgCol.implicitHeight + 20
                                radius: 14
                                color: fromMe ? page.theme.bubbleOut : page.theme.bubbleIn

                                Column {
                                    id: msgCol
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: 12
                                    spacing: 4

                                    Label {
                                        visible: !fromMe && !isFile
                                        text: sender
                                        color: page.theme.accent
                                        font.pixelSize: 12
                                        font.bold: true
                                    }

                                    Label {
                                        visible: !isFile
                                        width: parent.width
                                        text: msgDelegate.text
                                        color: page.theme.text
                                        font.pixelSize: 13
                                        wrapMode: Text.Wrap
                                    }

                                    // File card
                                    Rectangle {
                                        visible: isFile
                                        width: parent.width
                                        height: 56
                                        radius: 10
                                        color: Qt.rgba(0, 0, 0, 0.2)

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 8
                                            spacing: 8

                                            Rectangle {
                                                Layout.preferredWidth: 36
                                                Layout.preferredHeight: 36
                                                radius: 8
                                                color: page.theme.accentSoft
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "📎"
                                                    font.pixelSize: 14
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 2
                                                Label {
                                                    text: fileName
                                                    color: page.theme.text
                                                    font.pixelSize: 12
                                                    font.bold: true
                                                    elide: Text.ElideMiddle
                                                    Layout.fillWidth: true
                                                }
                                                Label {
                                                    text: fileSize
                                                    color: page.theme.textMuted
                                                    font.pixelSize: 11
                                                }
                                            }

                                            Rectangle {
                                                Layout.preferredWidth: 28
                                                Layout.preferredHeight: 28
                                                radius: 14
                                                color: page.theme.accent
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "↓"
                                                    color: "white"
                                                    font.pixelSize: 14
                                                }
                                            }
                                        }
                                    }

                                    Label {
                                        anchors.right: fromMe ? parent.right : undefined
                                        text: time + (fromMe ? "  ✓✓" : "")
                                        color: fromMe ? Qt.rgba(1, 1, 1, 0.7) : page.theme.textDim
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: page.peerMode
                        text: qsTr("No messages with %1 yet.\nMessaging arrives in Phase 3.").arg(Chat.activePeerName)
                        horizontalAlignment: Text.AlignHCenter
                        color: page.theme.textDim
                        font.pixelSize: 13
                    }

                    footer: Row {
                        width: messageList.width - 40
                        leftPadding: 40
                        spacing: 8
                        visible: !page.peerMode

                        Rectangle {
                            width: 48
                            height: 28
                            radius: 14
                            color: page.theme.bubbleIn

                            Row {
                                anchors.centerIn: parent
                                spacing: 3
                                Repeater {
                                    model: 3
                                    Rectangle {
                                        width: 5
                                        height: 5
                                        radius: 2.5
                                        color: page.theme.textMuted
                                    }
                                }
                            }
                        }
                    }
                }

                // Composer
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    color: page.theme.surface

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 12
                        radius: 24
                        color: page.theme.surfaceAlt
                        border.color: page.theme.border
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 8

                            Text {
                                text: "📎"
                                font.pixelSize: 16
                                color: page.theme.textMuted
                            }
                            Text {
                                text: "☺"
                                font.pixelSize: 16
                                color: page.theme.textMuted
                            }

                            TextInput {
                                id: composer
                                Layout.fillWidth: true
                                color: page.theme.text
                                font.pixelSize: 13
                                clip: true
                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    text: qsTr("Write a message...")
                                    color: page.theme.textDim
                                    font.pixelSize: 13
                                    visible: !composer.text && !composer.activeFocus
                                }
                                Keys.onReturnPressed: sendBtn.clicked()
                            }

                            Rectangle {
                                id: sendBtn
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                radius: 20
                                color: page.theme.accent

                                signal clicked()
                                Text {
                                    anchors.centerIn: parent
                                    text: "➤"
                                    color: "white"
                                    font.pixelSize: 16
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (composer.text.trim().length === 0 || page.peerMode)
                                            return
                                        messageModel.append({
                                            fromMe: true,
                                            sender: "You",
                                            initials: "JD",
                                            avatarColor: "#2563EB",
                                            text: composer.text.trim(),
                                            time: Qt.formatTime(new Date(), "hh:mm"),
                                            isFile: false,
                                            fileName: "",
                                            fileSize: ""
                                        })
                                        composer.text = ""
                                        messageList.positionViewAtEnd()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: page.theme.border
        }

        // ── 4. Right utility panel ───────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: page.theme.surface

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                // Video call card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    radius: page.theme.radius
                    color: "#0A0D14"
                    clip: true

                    // Remote video placeholder
                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#1E293B" }
                            GradientStop { position: 1.0; color: "#0F172A" }
                        }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 64
                        height: 64
                        radius: 32
                        color: "#3B82F6"
                        Text {
                            anchors.centerIn: parent
                            text: "AS"
                            color: "white"
                            font.bold: true
                            font.pixelSize: 20
                        }
                    }

                    Label {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 10
                        text: qsTr("Alice Smith")
                        color: "white"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 8
                        width: callBadge.implicitWidth + 16
                        height: 22
                        radius: 11
                        color: Qt.rgba(0, 0, 0, 0.55)
                        Label {
                            id: callBadge
                            anchors.centerIn: parent
                            text: qsTr("In call • 12:34")
                            color: "white"
                            font.pixelSize: 10
                        }
                    }

                    // PiP self view
                    Rectangle {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 8
                        width: 88
                        height: 64
                        radius: 8
                        color: "#1E293B"
                        border.color: page.theme.border
                        border.width: 1

                        Rectangle {
                            anchors.centerIn: parent
                            width: 28
                            height: 28
                            radius: 14
                            color: page.theme.accentSoft
                            Text {
                                anchors.centerIn: parent
                                text: "JD"
                                color: page.theme.accent
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        Label {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            anchors.margins: 4
                            text: qsTr("You")
                            color: "white"
                            font.pixelSize: 9
                        }
                    }
                }

                // Participants
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    radius: page.theme.radius
                    color: page.theme.surfaceAlt
                    border.color: page.theme.border
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Label {
                            text: qsTr("Participants (2)")
                            color: page.theme.text
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Repeater {
                            model: [
                                { name: "Alice Smith", initials: "AS", color: "#3B82F6", status: qsTr("Speaking") },
                                { name: "You", initials: "JD", color: "#2563EB", status: qsTr("Online") }
                            ]
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 8

                                Rectangle {
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    radius: 16
                                    color: modelData.color
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.initials
                                        color: "white"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label {
                                        text: modelData.name
                                        color: page.theme.text
                                        font.pixelSize: 12
                                    }
                                    Label {
                                        text: modelData.status
                                        color: modelData.status === qsTr("Speaking")
                                               ? page.theme.success : page.theme.textMuted
                                        font.pixelSize: 10
                                    }
                                }

                                Text { text: "🎤"; font.pixelSize: 12 }
                                Text { text: "📷"; font.pixelSize: 12 }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        Label {
                            text: qsTr("Show all")
                            color: page.theme.accent
                            font.pixelSize: 12
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: page.StackView.view.push(Qt.resolvedUrl("MeetingView.qml"))
                            }
                        }
                    }
                }

                // Transfers
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: page.theme.radius
                    color: page.theme.surfaceAlt
                    border.color: page.theme.border
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        Label {
                            text: qsTr("Transfers (2)")
                            color: page.theme.text
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Repeater {
                            model: [
                                {
                                    name: "Project_Design.zip",
                                    dir: "up",
                                    progress: 0.5,
                                    detail: "6.2 / 12.4 MB",
                                    accent: "#3B82F6"
                                },
                                {
                                    name: "Video_Intro.mp4",
                                    dir: "down",
                                    progress: 0.44,
                                    detail: "38 / 86 MB",
                                    accent: "#22C55E"
                                }
                            ]

                            delegate: ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Rectangle {
                                        Layout.preferredWidth: 28
                                        Layout.preferredHeight: 28
                                        radius: 8
                                        color: modelData.dir === "up" ? "#1E3A5F" : "#14532D"
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.dir === "up" ? "↑" : "↓"
                                            color: modelData.accent
                                            font.pixelSize: 14
                                            font.bold: true
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Label {
                                            text: modelData.name
                                            color: page.theme.text
                                            font.pixelSize: 12
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: modelData.detail
                                            color: page.theme.textMuted
                                            font.pixelSize: 10
                                        }
                                    }

                                    Text {
                                        text: "✕"
                                        color: page.theme.textDim
                                        font.pixelSize: 12
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 4
                                    radius: 2
                                    color: page.theme.border

                                    Rectangle {
                                        width: parent.width * modelData.progress
                                        height: parent.height
                                        radius: 2
                                        color: modelData.accent
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }
}
