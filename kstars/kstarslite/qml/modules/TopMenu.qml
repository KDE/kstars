import QtQuick 2.0
import QtQuick.Layouts 1.1
import "../constants" 1.0
import "helpers"

ColumnLayout {
    id: topMenu
    property int padding: num.dp * 10
    property int minY: - menuRect.radius
    property int maxY: - menuRect.height
    y: maxY

    states: [
        State {
            name: "open"
            PropertyChanges {
                target: topMenu
                y: minY
            }
            PropertyChanges {
                target: arrowDown
                state:""
            }
        },
        State {
            name: ""
            PropertyChanges {
                target: topMenu
                y: maxY
            }
            PropertyChanges {
                target: arrowDown
                state:"visible"
            }
        }
    ]

    Behavior on y {
        PropertyAnimation {
            duration: 500
        }
    }

    Rectangle {
        id: menuRect

        width: menuGrid.width + padding*3
        height: menuGrid.height + padding*3 + radius
        y: maxY
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
                top: parent.top
                topMargin: padding + parent.radius
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
                iconSrc: num.iconpath + "/sc-actions-kstars_stars.png"
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_deepsky.png"
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        KStarsLite.fullUpdate()
                    }
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_planets.png"
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        KStarsLite.fullUpdate()
                    }
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_clines.png"
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_cnames.png"
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_cbound.png"
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_mw.png"
            }
            TopMenuButton {
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        SkyMapLite.slotZoomIn()
                        KStarsLite.fullUpdate()
                    }
                }
                iconSrc: num.iconpath + "/sc-actions-kstars_grid.png"
            }
            TopMenuButton {
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        SkyMapLite.slotZoomOut()
                        KStarsLite.fullUpdate()
                    }
                }
                iconSrc: num.iconpath + "/sc-actions-kstars_horizon.png"
            }
        }

    }

    Image {
        id: arrowDown
        anchors {
            top: menuRect.bottom
            horizontalCenter: menuRect.horizontalCenter
            topMargin: num.dp * 10
        }
        source: num.imagesPath + "arrow_down.png"

        Component.onCompleted: {
            state = "visible"
        }

        MouseArea {
            anchors.fill: parent
            onPressed: {
                topMenu.state = topMenu.state == "open" ? "" : "open"
            }
        }

        states: [
            State {
                name:"visible"
                PropertyChanges {
                    target: arrowDown
                    opacity: 1
                    visible: true
                }
            },
            State {
                name:""
                PropertyChanges {
                    target: arrowDown
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
                        target: arrowDown
                        property: "opacity"
                        duration: 300
                    }
                    NumberAnimation {
                        target: arrowDown
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
                        target: arrowDown
                        property: "visible"
                        duration: 0
                    }
                    NumberAnimation {
                        target: arrowDown
                        property: "opacity"
                        duration: 300
                    }
                }
            }
        ]
    }
}
