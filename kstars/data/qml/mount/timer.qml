import QtQuick 2.15

Timer {
    running: false
    repeat: false

    property var callback
    onTriggered: callback()
}
