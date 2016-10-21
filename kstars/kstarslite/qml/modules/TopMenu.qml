// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

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
    property string prevState

    Connections {
        target: SkyMapLite
        onSlewingChanged: {
            if(SkyMapLite.slewing) {
                prevState = state
                state = "hidden"
            } else {
                state = prevState
            }
        }
    }

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
        },
        State {
            name: "hidden"
            PropertyChanges {
                target: topMenu
                y: -topMenu.height
            }
        }
    ]

    transitions: [
        Transition {
            to: "open"
            PropertyAnimation { target: topMenu
                properties: "y"; duration: 300 }
        },
        Transition {
            to: "closed"
            PropertyAnimation { target: topMenu
                properties: "y"; duration: 300 }
        },
        Transition {
            to: "hidden"
            PropertyAnimation { target: topMenu
                properties: "y"; duration: 200 }
        }
    ]

    Pane {
        id: topBar
        anchors.horizontalCenter: parent.horizontalCenter
        implicitWidth: parent.width
        Layout.fillHeight: true

        background: Rectangle {
            id: menuRect
            color: num.sysPalette.base
            border {
                width: 2
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
                childrenWidth += topBar.padding*2 //Account for topBar padding to have enough space for all elements
            }

            anchors {
                top: parent.top
                topMargin: menuRect.radius/2 // Center vertically menuGrid in background rectangle
            }
            Layout.fillHeight: true

            TopMenuButton {
                iconSrc: "../../images/kstars_stars.png"
                title: "Stars"

                toggled: KStarsLite.isToggled(ObjectsToToggle.Stars)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Stars, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_deepsky.png"
                title: "DeepSky Objects"

                toggled: KStarsLite.isToggled(ObjectsToToggle.DeepSky)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.DeepSky, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_planets.png"
                title: "Solar System"
                titlePlural: false

                toggled: KStarsLite.isToggled(ObjectsToToggle.Planets)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Planets, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_supernovae.png"
                title: "Supernovae"

                toggled: KStarsLite.isToggled(ObjectsToToggle.Supernovae)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Supernovae, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_satellites.png"
                title: "Satellites"

                toggled: KStarsLite.isToggled(ObjectsToToggle.Satellites)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Satellites, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_clines.png"
                title: "Constellation Lines"

                toggled: KStarsLite.isToggled(ObjectsToToggle.CLines)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.CLines, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_cnames.png"
                title: "Constellation Names"

                toggled: KStarsLite.isToggled(ObjectsToToggle.CNames)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.CNames, toggled)
                }
            }

            TopMenuButton {
                iconSrc: "../../images/kstars_constellationart.png"
                title: "Constellation Art"
                titlePlural: false

                toggled: KStarsLite.isToggled(ObjectsToToggle.ConstellationArt)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.ConstellationArt, toggled)
                }
            }

            TopMenuButton {
                iconSrc: "../../images/kstars_cbound.png"
                title: "Constellation Bounds"

                toggled: KStarsLite.isToggled(ObjectsToToggle.CBounds)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.CBounds, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_mw.png"
                title: "Milky Way"
                titlePlural: false

                toggled: KStarsLite.isToggled(ObjectsToToggle.MilkyWay)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.MilkyWay, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_grid.png"
                title: "Equatorial Grid"
                titlePlural: false

                toggled: KStarsLite.isToggled(ObjectsToToggle.EquatorialGrid)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.EquatorialGrid, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_hgrid.png"
                title: "Horizontal Grid"
                titlePlural: false

                toggled: KStarsLite.isToggled(ObjectsToToggle.HorizontalGrid)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.HorizontalGrid, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_horizon.png"
                title: "Horizon"
                titlePlural: false

                toggled: KStarsLite.isToggled(ObjectsToToggle.Ground)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Ground, toggled)
                }
            }
        }
    }

    Image {
        id: arrowDown
        anchors {
            horizontalCenter: parent.horizontalCenter
        }
        state: "open"
        source: "../images/arrow.png"
        rotation: {
            if(topMenu.state == "closed")
                return 180
            else if(topMenu.state == "open")
                return 0
            return rotation //If it state is "hidden" return current rotation
        }

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
