import QtQuick 2.2

Rectangle {
    id: slide

    Text {
        x: 73
        y: -62
        color: "#fd2121"
        anchors.centerIn: parent
        text: loader.modelData.slideTitle
        font.pointSize: 21
        anchors.verticalCenterOffset: -134
        anchors.horizontalCenterOffset: -3
    }
}
