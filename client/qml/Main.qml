import QtQuick
import QtQuick.Controls
import QtMeet

ApplicationWindow {
    id: window
    width: 1100
    height: 720
    visible: true
    title: qsTr("QtMeet")

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: LoginPage {}
    }
}
