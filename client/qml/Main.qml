import QtQuick
import QtQuick.Controls
import QtMeet
import "theme"

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: qsTr("QtMeet")
    color: theme.bg

    readonly property Theme theme: Theme {}

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: LoginPage {}
    }
}
