import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMeet
import "../theme"

Item {
    id: root

    property bool open: false
    property string title: qsTr("In-call messages")

    readonly property Theme theme: Theme {}

    function bindChat() {
        // 1:1 call → direct peer chat; meeting → shared conversation.
        if (Call.state === 4 && Call.meetingConversationId !== 0) {
            if (Chat.activeConversationId !== Call.meetingConversationId)
                Chat.openConversation(Call.meetingConversationId, Call.meetingTitle || qsTr("Meeting"))
        } else if ((Call.state === 1 || Call.state === 2 || Call.state === 3) && Call.peerId !== 0) {
            if (Chat.activePeerId !== Call.peerId)
                Chat.openConversationWith(Call.peerId, Call.peerName || qsTr("Call"))
        }
    }

    function sendCurrent() {
        const text = input.text
        if (!text.trim().length || !Chat.canSend)
            return
        Chat.sendText(text)
        input.clear()
        Qt.callLater(function() { list.positionViewAtEnd() })
    }

    onOpenChanged: {
        if (open) {
            bindChat()
            Qt.callLater(function() { input.forceActiveFocus() })
        }
    }

    Connections {
        target: Call
        function onStateChanged() {
            if (root.open)
                root.bindChat()
        }
        function onMeetingChanged() {
            if (root.open)
                root.bindChat()
        }
    }

    Connections {
        target: Chat
        function onIncomingMessage(fromName, text) {
            if (!root.open && text.length)
                root.open = true
        }
    }

    // Dim video only (leave bottom control bar clickable).
    Rectangle {
        anchors.left: parent.left
        anchors.right: panel.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 88
        visible: root.open
        z: 1
        color: "#66000000"
        MouseArea {
            anchors.fill: parent
            onClicked: root.open = false
        }
    }

    Rectangle {
        id: panel
        width: Math.min(360, parent.width * 0.92)
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 88
        anchors.right: parent.right
        anchors.rightMargin: root.open ? 0 : -width - 8
        z: 2
        color: theme.surface
        border.color: theme.border
        border.width: 1

        Behavior on anchors.rightMargin {
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: root.title
                    color: theme.text
                    font.bold: true
                    font.pixelSize: 15
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                ToolButton {
                    text: qsTr("Close")
                    onClicked: root.open = false
                }
            }

            Label {
                visible: Call.state === 4
                text: qsTr("Everyone in this meeting can see messages")
                color: theme.textMuted
                font.pixelSize: 11
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: Chat.messages
                verticalLayoutDirection: ListView.TopToBottom

                onCountChanged: Qt.callLater(function() { list.positionViewAtEnd() })

                delegate: Item {
                    id: row
                    required property var model
                    width: ListView.view.width
                    height: bubble.implicitHeight

                    Rectangle {
                        id: bubble
                        anchors.left: model.fromMe ? undefined : parent.left
                        anchors.right: model.fromMe ? parent.right : undefined
                        width: Math.min(parent.width * 0.88, Math.max(80, msgLabel.implicitWidth + 20))
                        implicitHeight: msgCol.implicitHeight + 16
                        radius: theme.radiusSm
                        color: model.fromMe ? theme.accentSoft : theme.surfaceAlt
                        border.color: theme.border

                        Column {
                            id: msgCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 10
                            spacing: 2

                            Label {
                                visible: !model.fromMe && Call.state === 4
                                width: parent.width
                                text: qsTr("User %1").arg(model.fromUserId)
                                color: theme.accent
                                font.pixelSize: 11
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                id: msgLabel
                                width: parent.width
                                text: model.text.length ? model.text
                                                        : (model.fileName.length ? qsTr("📎 %1").arg(model.fileName)
                                                                                 : qsTr("(empty)"))
                                color: theme.text
                                wrapMode: Text.Wrap
                                font.pixelSize: 13
                                opacity: model.pending ? 0.65 : 1
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    text: qsTr("No messages yet")
                    color: theme.textMuted
                }
            }

            Button {
                visible: Chat.hasMoreMessages
                text: qsTr("Load earlier")
                Layout.alignment: Qt.AlignHCenter
                onClicked: Chat.loadMore()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: input
                    Layout.fillWidth: true
                    placeholderText: Chat.canSend ? qsTr("Send a message")
                                                  : qsTr("Chat unavailable")
                    enabled: Chat.canSend
                    color: theme.text
                    onAccepted: root.sendCurrent()
                }
                Button {
                    text: qsTr("Send")
                    enabled: Chat.canSend && input.text.trim().length > 0
                    onClicked: root.sendCurrent()
                }
            }
        }
    }
}
