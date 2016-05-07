import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0
import "helpers"
import "../constants" 1.0

Item {
    id: contextDrawer
    property int dragWidth: num.dp * 50
    property int minX: parent.width - width
    property int maxX: parent.width - globShadow.anchors.leftMargin - dragWidth
    property double bgOpacity: 1 + (x - minX)/(minX-maxX)
    x: maxX

    states: [
        State {
            name: "open"
            PropertyChanges {
                target: contextDrawer
                x: minX
            }
        },
        State {
            name: ""
            PropertyChanges {
                target: contextDrawer
                x: maxX
            }
        }
    ]

    Behavior on x {
        PropertyAnimation {
            duration: 150
        }
    }

    MouseArea {
        id: globDrag
        anchors.fill: parent
        drag {
            target: contextDrawer
            axis: Drag.XAxis
            minimumX:  contextDrawer.minX
            maximumX: contextDrawer.maxX
            onActiveChanged: {
                if (contextDrawer.state == "") {
                    if (contextDrawer.x < maxX*0.8) {
                        contextDrawer.state = "open"
                    }
                    else contextDrawer.x = maxX
                }
                else if (contextDrawer.state == "open"){
                    if (contextDrawer.x > minX + maxX*0.15) {
                        contextDrawer.state = ""
                    }
                    else contextDrawer.x = minX
                }
            }
        }

    }

    BorderImage {
        id: globShadow
        anchors {
            top: parentRect.top
            bottom: parentRect.bottom
            left: parentRect.left
        }
        anchors { topMargin: -2; leftMargin: -8; bottomMargin: -2 }
        border { top: 10; right: 10; bottom: 10 }
        source: "images/shadow.png"
    }

    Rectangle {
        anchors {
            fill: parent
            leftMargin: contextDrawer.dragWidth
        }

        id: parentRect

        ColumnLayout {
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                topMargin: num.dp * 10
                leftMargin: num.dp * 10
            }
            spacing: num.dp * 10

            Text {
                id: contextHeader
                text: "Mars"
                font.family: "Oxygen Normal"
                font.pixelSize: num.dp * 44
                color: "#31363b"
            }

            ColumnLayout {
                anchors {
                    top: contextHeader.bottom
                    topMargin: num.dp * 30
                }
                spacing: num.dp * 30


                DrawerElem {
                    iconSrc: num.iconpath + "/compass.png"
                    title: "Center & Track"
                }

                DrawerElem {
                    iconSrc: num.iconpath + "/games-config-custom.png"
                    title: "Details"
                }
            }
        }
    }
}
