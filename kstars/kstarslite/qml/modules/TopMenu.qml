// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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
    objectName: "topMenu"
    property int padding: 10
    property double openOffset: -topBar.background.radius //Hide top round corners
    property double closedOffset: -topBar.height // Hide top bar when closed
    property string prevState

    Connections {
        target: SkyMapLite
        onSlewingChanged: {
            if(SkyMapLite.slewing || skyMapLite.automaticMode) {
                prevState = state
                state = "hidden"
            } else {
                state = prevState
            }
        }
    }

    state: "closed"
    property alias state : topMenu.state
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
            color: Num.sysPalette.base
            border {
                width: 2
                color: Num.sysPalette.light
            }
            radius: 10
        }

        Flow {
            id: menuFlow
            spacing: 5
            width: parent.width
            property double childrenWidth: 0

            Component.onCompleted: {
                if(Qt.platform.os == "android") {
                    //Automatic mode is available only for Android
                    var columnForTab = Qt.createQmlObject('import QtQuick 2.7
                                                            import "helpers"
                TopMenuButton {
                    id: autoModeButton
                    iconSrc: "../../images/kstars_automode.png"
                    title: xi18n("Automatic mode")
                    titlePlural: false
                    visible: Qt.platform.os == "android"

                    toggled: SkyMapLite.automaticMode
                    onClicked: {
                        SkyMapLite.automaticMode = !SkyMapLite.automaticMode
                    }
                }', menuFlow)
                    }

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
                title: xi18n("Stars")

                toggled: KStarsLite.isToggled(ObjectsToToggle.Stars)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Stars, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_deepsky.png"
                title: xi18n("DeepSky Objects")

                toggled: KStarsLite.isToggled(ObjectsToToggle.DeepSky)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.DeepSky, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_planets.png"
                title: xi18n("Solar System")
                titlePlural: false

                toggled: KStarsLite.isToggled(ObjectsToToggle.Planets)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Planets, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_supernovae.png"
                title: xi18n("Supernovae")

                toggled: KStarsLite.isToggled(ObjectsToToggle.Supernovae)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Supernovae, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_satellites.png"
                title: xi18n("Satellites")

                toggled: KStarsLite.isToggled(ObjectsToToggle.Satellites)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.Satellites, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_clines.png"
                title: xi18n("Constellation Lines")

                toggled: KStarsLite.isToggled(ObjectsToToggle.CLines)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.CLines, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_cnames.png"
                title: xi18n("Constellation Names")

                toggled: KStarsLite.isToggled(ObjectsToToggle.CNames)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.CNames, toggled)
                }
            }

            TopMenuButton {
                iconSrc: "../../images/kstars_constellationart.png"
                title: xi18n("Constellation Art")
                titlePlural: false

                toggled: KStarsLite.isToggled(ObjectsToToggle.ConstellationArt)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.ConstellationArt, toggled)
                }
            }

            TopMenuButton {
                iconSrc: "../../images/kstars_cbound.png"
                title: xi18n("Constellation Bounds")

                toggled: KStarsLite.isToggled(ObjectsToToggle.CBounds)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.CBounds, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_mw.png"
                title: xi18n("Milky Way")
                titlePlural: false

                toggled: KStarsLite.isToggled(ObjectsToToggle.MilkyWay)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.MilkyWay, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_grid.png"
                title: xi18n("Equatorial Grid")
                titlePlural: false

                toggled: KStarsLite.isToggled(ObjectsToToggle.EquatorialGrid)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.EquatorialGrid, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_hgrid.png"
                title: xi18n("Horizontal Grid")
                titlePlural: false

                toggled: KStarsLite.isToggled(ObjectsToToggle.HorizontalGrid)
                onClicked: {
                    KStarsLite.toggleObjects(ObjectsToToggle.HorizontalGrid, toggled)
                }
            }
            TopMenuButton {
                iconSrc: "../../images/kstars_horizon.png"
                title: xi18n("Horizon")
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
            objectName: "arrowDownMouseArea"
            anchors.fill: parent
            onPressed: {
                topMenu.state = topMenu.state == "closed" ? "open" : "closed"
            }
            function manualPress() {
                onPressed(1);
            }
        }

        Behavior on rotation {
            RotationAnimation {
                duration: 200; direction: RotationAnimation.Counterclockwise
            }
        }
    }
}
