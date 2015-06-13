import QtQuick 2.2

Rectangle {
    id: window
    property alias mouseArea: mouseArea
    x: 0
    y: 0
    
    width: 360
    height: 360
    
    MouseArea {
        id: mouseArea
        anchors.bottomMargin: 0
        anchors.leftMargin: 0
        anchors.topMargin: 0
        anchors.rightMargin: 0
        anchors.fill: parent
    }
    
    Text {
        color: "#fd2121"
        anchors.centerIn: parent
        text: "Sky Guide"
        font.pointSize: 21
        anchors.verticalCenterOffset: -130
        anchors.horizontalCenterOffset: 1
    }
}
