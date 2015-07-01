import QtQuick 2.2

Rectangle {
    id: info

    Text {
        text: loader.modelData.title
        color: "#fd2121"
        font.pointSize: 21
        horizontalAlignment: Text.AlignHCenter
        anchors.fill: parent
    }
}
