import QtQuick 2.2

Rectangle {
    id: slide

    Text {
        text: loader.modelData.slideTitle
        color: "#fd2121"
        font.pixelSize: fontSizeHeader
        horizontalAlignment: Text.AlignHCenter
        anchors.fill: parent
    }
}
