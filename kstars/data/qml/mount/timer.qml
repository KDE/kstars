import QtQuick

Timer {
    running: false
    repeat: false

    property var callback
    onTriggered: callback()
}
