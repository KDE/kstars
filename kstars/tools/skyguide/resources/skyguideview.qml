import QtQuick 2.2

Rectangle {
    id: window
    x: 0
    y: 0
    
    width: 360
    height: 360
    
    Text {
        color: "#fd2121"
        anchors.centerIn: parent
        text: "Sky Guide"
        font.pointSize: 21
        anchors.verticalCenterOffset: -134
        anchors.horizontalCenterOffset: -3
    }

    Rectangle {
        id: guidesArea
        x: 33
        y: 91
        width: 287
        height: 251

        Component {
            id: guidesDelegate
            Item {
                width: guidesArea.width
                height: 25
                Column {
                    Text { text: title }
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: ListView.currentIndex = index
                }
            }
        }

        ListView {
            keyNavigationWraps: false
            boundsBehavior: Flickable.StopAtBounds
            anchors.fill: parent
            model: guidesModel
            highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
            focus: true
            delegate: guidesDelegate
        }
    }
}
