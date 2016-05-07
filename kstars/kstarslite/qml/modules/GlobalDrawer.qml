import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0
import "helpers"
import "../constants" 1.0

Item {
    id: globalDrawer
    property int dragWidth: num.dp * 50
    property int maxX: 0
    property int minX: dragWidth - width + globShadow.anchors.rightMargin
    property double bgOpacity: 1 - (x/minX)
    x: minX

    states: [
        State {
            name: "open"
            PropertyChanges {
                target: globalDrawer
                x: maxX
            }
        },
        State {
            name: ""
            PropertyChanges {
                target: globalDrawer
                x: minX
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
        anchors {
            fill: parent
        }

        drag {
            target: globalDrawer
            axis: Drag.XAxis
            minimumX: globalDrawer.minX
            maximumX: globalDrawer.maxX
            onActiveChanged: {
                if (globalDrawer.state == "") {
                    if (globalDrawer.x > minX*0.8) {
                        globalDrawer.state = "open"
                    }
                    else globalDrawer.x = minX
                }
                else if (globalDrawer.state == "open"){
                    if (globalDrawer.x < minX*0.3) {
                        globalDrawer.state = ""
                    }
                    else globalDrawer.x = maxX
                }
            }
        }


        BorderImage {
            id: globShadow
            anchors {
                top: parentRect.top
                bottom: parentRect.bottom
                right: parentRect.right
            }
            anchors { topMargin: -2; rightMargin: -8; bottomMargin: -2 }
            border { top: 10; right: 10; bottom: 10 }
            source: "images/shadow.png"
        }

        Rectangle {
            anchors {
                top: parent.top
                bottom: parent.bottom
            }
            width: parent.width - dragWidth

            id: parentRect

            Image {
                id: contextHeader
                source: "images/kstars.png"
                fillMode: Image.PreserveAspectFit
                width: parentRect.width
            }

            ColumnLayout {
                anchors {
                    left: parent.left
                    right: parent.right
                    top: contextHeader.bottom
                    topMargin: num.dp * 10
                    leftMargin: num.dp * 10
                }
                spacing: num.dp * 30

                DrawerElem {
                    iconSrc: num.iconpath + "/system-search.png"
                    title: "Search"
                }

                DrawerColorScheme {

                }

                DrawerElem {
                    iconSrc: num.iconpath + "/applications-system.png"
                    title: "Settings"
                }
            }

        }

    }
}
