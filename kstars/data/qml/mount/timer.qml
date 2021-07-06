import QtQuick 2.0

Timer {
    running: false
    repeat: false

    property var callback
    onTriggered: callback()
}
