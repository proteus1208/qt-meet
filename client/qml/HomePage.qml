import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMeet
import "theme"

Page {
    id: page

    readonly property Theme theme: Theme {}
    property int currentNav: 0 // 0 chats, 1 people, 2 meetings, 3 files, 4 live
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

    function formatBytes(n) {
        const v = Number(n)
        if (!isFinite(v) || v < 0)
            return ""
        if (v < 1024)
            return v + " B"
        if (v < 1024 * 1024)
            return (v / 1024).toFixed(1) + " KB"
        if (v < 1024 * 1024 * 1024)
            return (v / (1024 * 1024)).toFixed(1) + " MB"
        return (v / (1024 * 1024 * 1024)).toFixed(1) + " GB"
    }

    function formatTime(ms) {
        if (!ms)
            return ""
        const d = new Date(Number(ms))
        const hh = String(d.getHours()).padStart(2, "0")
        const mm = String(d.getMinutes()).padStart(2, "0")
        return hh + ":" + mm
    }

    FileDialog {
        id: attachDialog
        title: qsTr("Send file")
        fileMode: FileDialog.OpenFile
        onAccepted: Chat.sendFile(selectedFile)
    }

    JoinMeetingDialog {
        id: joinMeetingDialog
        onJoinRequested: function(code, video) {
            Call.joinMeeting(code, true, video)
        }
    }

    Dialog {
        id: createMeetingDialog
        title: qsTr("New meeting")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: Call.createMeeting(meetingTitleField.text.trim(), createWithCamera.checked)
        onOpened: {
            meetingTitleField.text = qsTr("Meeting")
            createWithCamera.checked = true
            meetingTitleField.forceActiveFocus()
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label { text: qsTr("Title") }
            TextField {
                id: meetingTitleField
                Layout.preferredWidth: 260
                placeholderText: qsTr("Team sync")
            }
            CheckBox {
                id: createWithCamera
                text: qsTr("Start with camera")
                checked: true
            }
        }
    }

    Connections {
        target: Call
        function onIncomingCall(callerName, video) {
            page.StackView.view.push(Qt.resolvedUrl("CallView.qml"))
        }
        function onStateChanged() {
            if (Call.state !== 4) // InMeeting
                return
            if (page.StackView.view.currentItem
                    && page.StackView.view.currentItem.objectName === "MeetingView")
                return
            page.StackView.view.push(Qt.resolvedUrl("MeetingView.qml"))
        }
        function onErrorOccurred(message) {
            meetingErrorLabel.text = message
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
                        { icon: "📁", label: qsTr("Files") },
                        { icon: "📺", label: qsTr("Live") }
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
                                    if (index === 3)
                                        page.StackView.view.push(Qt.resolvedUrl("ExplorerPage.qml"))
                                    else if (index === 4)
                                        page.StackView.view.push(Qt.resolvedUrl("ChannelsPage.qml"))
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
                    Layout.fillWidth: true
                    Layout.fillHeight: page.currentNav === 1
                    Layout.preferredHeight: page.currentNav === 1 ? -1 : 0
                    clip: true
                    spacing: 4
                    visible: page.currentNav === 1
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
                    Layout.fillWidth: true
                    Layout.fillHeight: page.currentNav !== 1
                    Layout.preferredHeight: page.currentNav !== 1 ? -1 : 0
                    clip: true
                    spacing: 4
                    visible: page.currentNav !== 1
                    model: Chat.conversations

                    delegate: Rectangle {
                        id: chatDelegate
                        required property var model

                        width: chatList.width
                        height: 64
                        radius: page.theme.radiusSm
                        color: page.peerMode && Chat.activePeerId === model.peerUserId
                               ? page.theme.surfaceHover
                               : (chatHover.hovered ? page.theme.surfaceAlt : "transparent")

                        HoverHandler { id: chatHover }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                radius: 20
                                color: page.avatarColorFor(chatDelegate.model.peerUserId)
                                Text {
                                    anchors.centerIn: parent
                                    text: page.initialsOf(chatDelegate.model.title)
                                    color: "white"
                                    font.bold: true
                                    font.pixelSize: 13
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true
                                        text: chatDelegate.model.title
                                        color: page.theme.text
                                        font.pixelSize: 14
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: page.formatTime(chatDelegate.model.lastTimestamp)
                                        color: page.theme.textDim
                                        font.pixelSize: 11
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true
                                        text: chatDelegate.model.lastText
                                        color: page.theme.textMuted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Rectangle {
                                        visible: chatDelegate.model.unread > 0
                                        width: Math.max(18, unreadLabel.implicitWidth + 8)
                                        height: 18
                                        radius: 9
                                        color: page.theme.accent
                                        Label {
                                            id: unreadLabel
                                            anchors.centerIn: parent
                                            text: chatDelegate.model.unread
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
                            onClicked: Chat.openConversationWith(chatDelegate.model.peerUserId,
                                                                 chatDelegate.model.title)
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: chatList.count === 0
                        text: qsTr("No conversations yet.\nOpen People to start chatting.")
                        horizontalAlignment: Text.AlignHCenter
                        color: page.theme.textDim
                        font.pixelSize: 13
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
                                text: page.peerMode ? Chat.activePeerName : qsTr("No chat selected")
                                color: page.peerMode ? page.theme.text : page.theme.textMuted
                                font.pixelSize: 16
                                font.bold: true
                            }
                            RowLayout {
                                visible: page.peerMode
                                spacing: 6
                                readonly property int presence: Chat.activePeerPresence
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
                            model: page.peerMode ? callActions : []
                            readonly property var callActions: [
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
                                        if (!page.peerMode)
                                            return
                                        const video = modelData.tip.indexOf("Video") >= 0
                                                      || modelData.tip.indexOf("video") >= 0
                                        Call.callUser(Chat.activePeerId, Chat.activePeerName, video)
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
                    model: Chat.messages
                    topMargin: 16
                    bottomMargin: 16
                    leftMargin: 20
                    rightMargin: 20
                    onCountChanged: Qt.callLater(function() { positionViewAtEnd() })

                    header: Item {
                        width: messageList.width - 40
                        height: Chat.hasMoreMessages ? 40 : 0
                        visible: height > 0
                        Button {
                            anchors.centerIn: parent
                            text: qsTr("Load earlier messages")
                            flat: true
                            onClicked: Chat.loadMore()
                        }
                    }

                    delegate: Column {
                        id: msgDelegate
                        required property bool fromMe
                        required property string text
                        required property var timestamp
                        required property bool pending
                        required property string fileId
                        required property string fileName
                        required property var fileSize
                        required property var fromUserId

                        readonly property bool isFile: fileId && fileId.length > 0
                        readonly property string sender: fromMe ? Session.displayName : Chat.activePeerName
                        readonly property string initials: page.initialsOf(sender)
                        readonly property string avatarColor: page.avatarColorFor(fromMe ? Session.userId : fromUserId)
                        readonly property string time: page.formatTime(timestamp)

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
                                opacity: pending ? 0.7 : 1

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
                                        visible: !isFile && msgDelegate.text.length > 0
                                        width: parent.width
                                        text: msgDelegate.text
                                        color: page.theme.text
                                        font.pixelSize: 13
                                        wrapMode: Text.Wrap
                                    }

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
                                                    text: page.formatBytes(fileSize)
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
                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: Chat.downloadAttachment(fileId, fileName)
                                                }
                                            }
                                        }
                                    }

                                    Label {
                                        anchors.right: fromMe ? parent.right : undefined
                                        text: time + (fromMe ? (pending ? "  …" : "  ✓✓") : "")
                                        color: fromMe ? Qt.rgba(1, 1, 1, 0.7) : page.theme.textDim
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: messageList.count === 0
                        text: page.peerMode
                              ? qsTr("No messages with %1 yet.\nSay hello!").arg(Chat.activePeerName)
                              : qsTr("Select a user on the left to start chatting.")
                        horizontalAlignment: Text.AlignHCenter
                        color: page.theme.textDim
                        font.pixelSize: 13
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
                                color: page.peerMode ? page.theme.textMuted : page.theme.textDim
                                opacity: page.peerMode ? 1 : 0.4
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: page.peerMode
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: attachDialog.open()
                                }
                            }

                            TextInput {
                                id: composer
                                Layout.fillWidth: true
                                color: page.theme.text
                                font.pixelSize: 13
                                clip: true
                                enabled: page.peerMode
                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    text: page.peerMode ? qsTr("Write a message...")
                                                        : qsTr("Select someone to message")
                                    color: page.theme.textDim
                                    font.pixelSize: 13
                                    visible: !composer.text && !composer.activeFocus
                                }
                                Keys.onReturnPressed: (event) => {
                                    if (event.modifiers & Qt.ShiftModifier) {
                                        event.accepted = false
                                        return
                                    }
                                    sendBtn.clicked()
                                    event.accepted = true
                                }
                            }

                            Rectangle {
                                id: sendBtn
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                radius: 20
                                color: page.theme.accent

                                signal clicked()
                                onClicked: {
                                    if (!page.peerMode || composer.text.trim().length === 0)
                                        return
                                    Chat.sendText(composer.text.trim())
                                    composer.text = ""
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: "➤"
                                    color: "white"
                                    font.pixelSize: 16
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: sendBtn.clicked()
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

                // Meetings
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 280
                    radius: page.theme.radius
                    color: page.theme.surfaceAlt
                    border.color: page.theme.border
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        Label {
                            text: qsTr("Meetings")
                            color: page.theme.text
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: page.theme.textMuted
                            font.pixelSize: 12
                            text: Call.meetingCode.length
                                  ? qsTr("Active code: %1").arg(Call.meetingCode)
                                  : qsTr("Create a room and share the code, or join with a code.")
                        }

                        Label {
                            text: qsTr("Bandwidth")
                            color: page.theme.text
                            font.pixelSize: 12
                            font.bold: true
                        }
                        ComboBox {
                            Layout.fillWidth: true
                            model: Call.bandwidthLabels
                            currentIndex: Call.bandwidthIndex
                            onActivated: Call.bandwidthIndex = index
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: page.theme.textDim
                            font.pixelSize: 11
                            text: qsTr("Server delivery: %1").arg(Session.transportModeLabel)
                        }

                        Label {
                            id: meetingErrorLabel
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: "#DC2626"
                            font.pixelSize: 11
                            visible: text.length > 0
                        }

                        Button {
                            Layout.fillWidth: true
                            text: qsTr("New meeting")
                            enabled: Session.loggedIn && Call.state === 0
                            onClicked: createMeetingDialog.open()
                        }
                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Join with code")
                            enabled: Session.loggedIn && Call.state === 0
                            onClicked: joinMeetingDialog.open()
                        }
                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Open meeting")
                            visible: Call.state === 4
                            onClicked: page.StackView.view.push(Qt.resolvedUrl("MeetingView.qml"))
                        }
                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Live channels")
                            enabled: Session.loggedIn && Call.state === 0
                            onClicked: page.StackView.view.push(Qt.resolvedUrl("ChannelsPage.qml"))
                        }
                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Open live")
                            visible: Call.state === 5
                            onClicked: page.StackView.view.push(Qt.resolvedUrl("ChannelWatchView.qml"))
                        }
                    }
                }

                // Participants (live during call/meeting)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    radius: page.theme.radius
                    color: page.theme.surfaceAlt
                    border.color: page.theme.border
                    border.width: 1
                    visible: Call.state === 3 || Call.state === 4

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Label {
                            text: qsTr("Participants")
                            color: page.theme.text
                            font.pixelSize: 13
                            font.bold: true
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: Call.participants
                            delegate: RowLayout {
                                width: ListView.view.width
                                spacing: 8
                                required property string displayName
                                required property bool audioOn
                                required property bool videoOn

                                Label {
                                    Layout.fillWidth: true
                                    text: displayName
                                    color: page.theme.text
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: audioOn ? "🎤" : "🔇"
                                    font.pixelSize: 12
                                }
                                Label {
                                    text: videoOn ? "📷" : "—"
                                    font.pixelSize: 12
                                }
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
                            text: qsTr("Transfers (%1)").arg(transfersView.count)
                            color: page.theme.text
                            font.pixelSize: 13
                            font.bold: true
                        }

                        ListView {
                            id: transfersView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 10
                            model: Chat.transfers

                            delegate: ColumnLayout {
                                required property string name
                                required property bool isUpload
                                required property var done
                                required property var total
                                required property double progress
                                required property int state

                                width: transfersView.width
                                spacing: 4

                                readonly property color accent: isUpload ? "#3B82F6" : "#22C55E"

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Rectangle {
                                        Layout.preferredWidth: 28
                                        Layout.preferredHeight: 28
                                        radius: 8
                                        color: isUpload ? "#1E3A5F" : "#14532D"
                                        Text {
                                            anchors.centerIn: parent
                                            text: isUpload ? "↑" : "↓"
                                            color: accent
                                            font.pixelSize: 14
                                            font.bold: true
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Label {
                                            text: name
                                            color: page.theme.text
                                            font.pixelSize: 12
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: page.formatBytes(done) + " / " + page.formatBytes(total)
                                            color: page.theme.textMuted
                                            font.pixelSize: 10
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 4
                                    radius: 2
                                    color: page.theme.border

                                    Rectangle {
                                        width: parent.width * Math.min(1, Math.max(0, progress))
                                        height: parent.height
                                        radius: 2
                                        color: accent
                                    }
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: transfersView.count === 0
                                text: qsTr("No active transfers")
                                color: page.theme.textDim
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }
    }
}
