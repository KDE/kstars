import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import "../constants" 1.0
import "helpers"

ColumnLayout {
    id: bottomMenu
    property int padding: num.dp * 10
    property int minY: - menuRect.height
    property int maxY: -menuRect.radius

    anchors{
        bottom: parent.bottom
        bottomMargin: minY
    }

    states: [
        State {
            name: "open"
            PropertyChanges {
                target: bottomMenu
                anchors.bottomMargin: maxY
            }
            PropertyChanges {
                target: arrowUp
                state:""
            }
        },
        State {
            name: ""
            PropertyChanges {
                target: bottomMenu
                y: minY
            }
            PropertyChanges {
                target: arrowUp
                state:"visible"
            }
        }
    ]

    Behavior on anchors.bottomMargin {
        PropertyAnimation {
            duration: 500
        }
    }

    Image {
            id: arrowUp
            anchors {
                bottom: menuRect.top
                horizontalCenter: menuRect.horizontalCenter
                bottomMargin: num.dp * 10
            }
            source: num.imagesPath + "arrow_up.png"

            Component.onCompleted: {
                state = "visible"
            }

            MouseArea {
                anchors.fill: parent
                onPressed: {
                    bottomMenu.state = bottomMenu.state == "open" ? "" : "open"
                }
            }

            states: [
                State {
                    name:"visible"
                    PropertyChanges {
                        target: arrowUp
                        opacity: 1
                        visible: true
                    }
                },
                State {
                    name:""
                    PropertyChanges {
                        target: arrowUp
                        opacity: 0
                        visible: false
                    }
                }
            ]

            transitions: [
                Transition {
                    from: "visible"
                    to: ""

                    SequentialAnimation {
                        NumberAnimation {
                            target: arrowUp
                            property: "opacity"
                            duration: 300
                        }
                        NumberAnimation {
                            target: arrowUp
                            property: "visible"
                            duration: 0
                        }
                    }
                },
                Transition {
                    from: ""
                    to: "visible"
                    SequentialAnimation {
                        NumberAnimation {
                            target: arrowUp
                            property: "visible"
                            duration: 0
                        }
                        NumberAnimation {
                            target: arrowUp
                            property: "opacity"
                            duration: 300
                        }
                    }
                }
            ]
        }

    Rectangle {
        id: menuRect

        width: menuGrid.width + padding*3
        height: menuGrid.height + padding*3 + radius
        color: "#000033"
        border {
            width: num.dp * 3
            color: "gray"
        }
        radius: num.dp * 20

        Grid {
            spacing: num.dp * 10
            id: menuGrid
            state: "landscape"
            anchors{
                bottom: parent.bottom
                bottomMargin: padding + parent.radius
                horizontalCenter: parent.horizontalCenter
            }

            states: [
                State {
                    name: "portrait"
                    PropertyChanges {
                        target: menuGrid
                        columns: 5
                    }
                },
                State {
                    name: "landscape"
                    PropertyChanges {
                        target: menuGrid
                        columns: 9
                    }
                }
            ]

            TopMenuButton {
                id: icon
                iconSrc: num.iconpath + "media-skip-backward.png"
            }
            TopMenuButton {
                iconSrc: num.iconpath + "media-playback-start.png"
            }
            TopMenuButton {
                iconSrc: num.iconpath + "media-skip-forward.png"
            }

            TextField {
                text: "1 sec."
                style: TextFieldStyle {
                    background: Rectangle {
                        color: "#000"
                        radius: 2
                        implicitWidth: 100
                        implicitHeight: 24
                        border.color: "gray"
                        border.width: 1
                    }
                }
                height: icon.height
                width: num.dp * 75
            }

            TopMenuButton {
                iconSrc: num.iconpath + "arrow-up.png"
            }
            TopMenuButton {
                iconSrc: num.iconpath + "arrow-down.png"
            }
        }

    }
}
