import QtQuick 2.7
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0

import QtQuick.Window 2.2
import "../constants" 1.0
import "helpers"
import KStarsLiteEnums 1.0

ColumnLayout {
    id: topMenu
    property int padding: 10
    property double openOffset: -topBar.background.radius //Hide top round corners
    property double closedOffset: -topBar.height // Hide top bar when closed

    state: "closed"
    spacing: padding

    x: (parent.width - width)/2

    Layout.fillHeight: true
    width: parent.width < menuFlow.childrenWidth  ? parent.width : menuFlow.childrenWidth

    states: [
        State {
            name: "open"
            PropertyChanges {
                target: topMenu
                y: openOffset
            }
        },
        State {
            name: "closed"
            PropertyChanges {
                target: topMenu
                y: closedOffset
            }
        }
    ]

    transitions: [
        Transition {
            from: "closed"; to: "open"
            PropertyAnimation { target: topMenu
                properties: "y"; duration: 300 }
        },
        Transition {
            from: "open"; to: "closed"
            PropertyAnimation { target: topMenu
                properties: "y"; duration: 300 }
        }
    ]

    Pane {
        id: topBar
        anchors.horizontalCenter: parent.horizontalCenter
        implicitWidth: parent.width
        Layout.fillHeight: true

        background: Rectangle {
            id: menuRect
            color: num.sysPalette.dark
            border {
                width: 1
                color: num.sysPalette.light
            }
            radius: 10
        }

        Flow {
            id: menuFlow
            spacing: 5
            width: parent.width
            property double childrenWidth: 0

            Component.onCompleted: {
                for(var i = 0; i < children.length; ++i) {
                    childrenWidth += children[i].width + spacing
                }
                childrenWidth += topBar.padding*2 //Acount for topBar padding to have enough space for all elements
            }

            anchors {
                top: parent.top
                topMargin: menuRect.radius/2 // Center vertically menuGrid in background rectangle
            }
            Layout.fillHeight: true

            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_stars.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.Stars)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Stars, toggled)
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_deepsky.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.DeepSky)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.DeepSky, toggled)
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_planets.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.Planets)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Planets, toggled)
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/supernovae.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.Supernovae)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Supernovae, toggled)
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/satellites.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.Satellites)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Satellites, toggled)
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_clines.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.CLines)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.CLines, toggled)
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_cnames.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.CNames)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.CNames, toggled)
                }
            }

            TopMenuButton {
                iconSrc: num.iconpath + "/constellationart.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.ConstellationArt)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.ConstellationArt, toggled)
                }
            }

            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_cbound.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.CBounds)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.CBounds, toggled)
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_mw.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.MilkyWay)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.MilkyWay, toggled)
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_grid.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.EquatorialGrid)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.EquatorialGrid, toggled)
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_hgrid.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.HorizontalGrid)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.HorizontalGrid, toggled)
                }
            }
            TopMenuButton {
                iconSrc: num.iconpath + "/sc-actions-kstars_horizon.png"

                toggled: KStarsLite.isToggled(ObjectsToToggle.Ground)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Ground, toggled)
                }
            }
        }
    }

    Item {
        width: arrowDown.sourceSize.width/num.pixelRatio
        height: arrowDown.sourceSize.height/num.pixelRatio
        anchors.horizontalCenter: parent.horizontalCenter

        Image {
            id: arrowDown
            anchors {
                fill: parent
                horizontalCenter: parent.horizontalCenter
            }
            width: sourceSize.width/num.pixelRatio
            height: sourceSize.height/num.pixelRatio
            state: "open"
            source: num.imagesPath + "arrow.png"
            rotation: topMenu.state == "closed" ? 0 : 180

            //transform: Rotation { axis { x: 1; y: 0; z: 0 } angle: 90 }
            //rotation: 180

            MouseArea {
                anchors.fill: parent
                onPressed: {
                    topMenu.state = topMenu.state == "closed" ? "open" : "closed"
                }
            }

            Behavior on rotation {
                RotationAnimation {
                    duration: 200; direction: RotationAnimation.Counterclockwise
                }
            }
        }
    }
}
