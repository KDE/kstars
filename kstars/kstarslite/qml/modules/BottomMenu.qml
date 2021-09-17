// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.7
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0
import QtQuick.Window 2.2

import TelescopeLiteEnums 1.0
import "../constants" 1.0
import "helpers"

ColumnLayout {
    id: bottomMenu
    objectName: "bottomMenu"
    property int padding: 10
    property bool telescope: false
    property int slewCount: 1
    property alias sliderValue: slider.value

    property double openOffset: bottomMenu.height - bottomBar.background.radius //Hide bottom round corners
    property double closedOffset: arrowUp.height + padding
    property string prevState

    property bool isWindowWidthSmall: window.width < menuGrid.maxWidth

    //Hide on slew
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
    property alias state : bottomMenu.state
    spacing: padding

    x: (parent.width - width)/2

    Layout.fillHeight: true

    states: [
        State {
            name: "open"
            PropertyChanges {
                target: bottomMenu
                y: parent.height - openOffset
            }
        },
        State {
            name: "closed"
            PropertyChanges {
                target: bottomMenu
                y: parent.height - closedOffset
            }
        },
        State {
            name: "hidden"
            PropertyChanges {
                target: bottomMenu
                y: parent.height
            }
        }
    ]

    transitions: [
        Transition {
            to: "open"
            PropertyAnimation { target: bottomMenu
                properties: "y"; duration: 300 }
        },
        Transition {
            to: "closed"
            PropertyAnimation { target: bottomMenu
                properties: "y"; duration: 300 }
        },
        Transition {
            to: "hidden"
            PropertyAnimation { target: bottomMenu
                properties: "y"; duration: 200 }
        }
    ]

    Image {
        id: arrowUp
        anchors {
            horizontalCenter: parent.horizontalCenter
        }
        state: "open"
        source: "../images/arrow.png"
        rotation: {
            if(bottomMenu.state == "closed")
                return 0
            else if(bottomMenu.state == "open")
                return 180
            return rotation //If it state is "hidden" return current rotation
        }
        mirror: true // Make sure that arrows in both menus look symmetric

        MouseArea {
            objectName: "arrowUpMouseArea"
            anchors.fill: parent
            onPressed: {
                bottomMenu.state = bottomMenu.state == "closed" ? "open" : "closed"
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

    RowLayout {
        anchors {
            bottom: bottomBar.top
            horizontalCenter: parent.horizontalCenter
        }
        visible: bottomMenu.telescope

        Slider {
            id: slider
            background: Rectangle {
                    y: 11
                    implicitWidth: 200
                    implicitHeight: 4
                    width: control.availableWidth
                    height: implicitHeight
                    radius: 2
                    color: "#bdbebf"

                    Rectangle {
                        width: control.visualPosition * parent.width
                        height: parent.height
                        color: "#21be2b"
                        radius: 2
                    }
                }
            value: 0
            stepSize: 1
            from: 1
            to: bottomMenu.slewCount-1
            wheelEnabled: false
            snapMode: Slider.SnapOnRelease

            onValueChanged: {
                slewLabel.text = ClientManagerLite.getTelescope().getSlewRateLabels()[slider.value]
            }

            onPressedChanged: {
                if (slider.pressed) return

                ClientManagerLite.getTelescope().setSlewRate(slider.value)
            }
        }

        Label {
            id: slewLabel
            color: "#d0d0d0"
            width: 100
            text: ""
        }
    }

    Pane {
        id: bottomBar
        anchors.horizontalCenter: parent.horizontalCenter

        background: Rectangle {
            id: menuRect
            color: Num.sysPalette.base
            border {
                width: 1
                color: Num.sysPalette.light
            }
            radius: 10
        }

        GridLayout {
            id: menuGrid
            property double maxWidth: {width} // We make menuGrid smaller when window width is less than this value

            onWidthChanged: {
                if(width > maxWidth) maxWidth = width
            }

            anchors {
                bottom: parent.bottom
                bottomMargin: menuRect.radius/2 // Center vertically menuGrid in background rectangle
            }
            rows: isWindowWidthSmall ? 2 : 1
            flow: isWindowWidthSmall ? GridLayout.TopToBottom : GridLayout.LeftToRight

            Layout.fillWidth: true

            RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                anchors {
                    left: parent.left
                    right: parent.right
                }

                BottomMenuButton {
                    id: telescopeLeft
                    iconSrc: "../../images/telescope-left.png"
                    visible: bottomMenu.telescope
                    onPressed: {
                        ClientManagerLite.getTelescope().moveWE(TelescopeNS.MOTION_WEST, TelescopeCommand.MOTION_START)
                    }
                    onClicked: {
                        ClientManagerLite.getTelescope().moveWE(TelescopeNS.MOTION_WEST, TelescopeCommand.MOTION_STOP)
                    }
                }

                BottomMenuButton {
                    id: telescopeDown1
                    iconSrc: "../../images/telescope-down.png"
                    visible: bottomMenu.telescope && menuGrid.rows == 1
                    onPressed: {
                        ClientManagerLite.getTelescope().moveNS(TelescopeNS.MOTION_SOUTH, TelescopeCommand.MOTION_START)
                    }
                    onClicked: {
                        ClientManagerLite.getTelescope().moveNS(TelescopeNS.MOTION_SOUTH, TelescopeCommand.MOTION_STOP)
                    }
                }

                BottomMenuButton {
                    id: goBackwards
                    iconSrc: "../../images/media-skip-backward.png"
                    onClicked: {
                        KStarsLite.slotStepBackward()
                    }
                }

                BottomMenuButton {
                    id: startTimer
                    state: SimClock.isActive() ? "on" : "off"

                    states: [
                        State {
                            name: "on"
                            PropertyChanges {
                                target: startTimer
                                iconSrc: "../../images/media-playback-pause.png"
                            }
                        },
                        State {
                            name: "off"
                            PropertyChanges {
                                target: startTimer
                                iconSrc: "../../images/media-playback-start.png"
                            }
                        }
                    ]

                    onClicked: {
                        KStarsLite.slotToggleTimer()
                        if(SimClock.isActive()) {
                            startTimer.state = "on"
                        } else {
                            startTimer.state = "off"
                        }
                    }

                    Connections {
                        target: window
                        onIsSkyMapVisibleChanged: {
                            if(!isSkyMapVisible && SimClock.isActive()) {
                                KStarsLite.slotToggleTimer()
                                startTimer.state = "off"
                            }
                        }
                    }
                }

                BottomMenuButton {
                    iconSrc: "../../images/media-skip-forward.png"
                    onClicked: {
                        KStarsLite.slotStepForward()
                    }
                }

                RowLayout {
                    anchors.right: parent.right

                    BottomMenuButton {
                        onClicked: {
                            stackView.push(timePage)
                        }
                        visible: isWindowWidthSmall

                        iconSrc: "../../images/appointment-new.png"
                    }

                    Rectangle {
                        id: separatorSearchSmall
                        height: decreaseUnitLandscape.height*0.75
                        color: Num.sysPalette.shadow
                        width: 1
                        visible: isWindowWidthSmall
                    }

                    BottomMenuButton {
                        onClicked: {
                            stackView.push(findDialog)
                        }
                        visible: isWindowWidthSmall

                        iconSrc: "../../images/edit-find.png"
                    }

                    BottomMenuButton {
                        id: telescopeRight1
                        iconSrc: "../../images/telescope-right.png"
                        visible: bottomMenu.telescope && menuGrid.rows == 2
                        onPressed: {
                            ClientManagerLite.getTelescope().moveWE(TelescopeNS.MOTION_EAST, TelescopeCommand.MOTION_START)
                        }
                        onClicked: {
                            ClientManagerLite.getTelescope().moveWE(TelescopeNS.MOTION_EAST, TelescopeCommand.MOTION_STOP)
                        }
                    }
                }
            }

            RowLayout {
                id: secondRow
                Layout.fillHeight: true
                Layout.fillWidth: true

                BottomMenuButton {
                    id: telescopeDown2
                    iconSrc: "../../images/telescope-down.png"
                    visible: bottomMenu.telescope && menuGrid.rows == 2
                    onPressed: {
                        ClientManagerLite.getTelescope().moveNS(TelescopeNS.MOTION_SOUTH, TelescopeCommand.MOTION_START)
                    }
                    onClicked: {
                        ClientManagerLite.getTelescope().moveNS(TelescopeNS.MOTION_SOUTH, TelescopeCommand.MOTION_STOP)
                    }
                }

                BottomMenuButton {
                    onClicked: {
                        timeSpinBox.decreaseTimeUnit()
                    }
                    visible: isWindowWidthSmall

                    iconSrc: "../../images/arrow-down.png"
                }

                TimeSpinBox {
                    id: timeSpinBox
                }

                BottomMenuButton {
                    id: increaseUnit
                    onClicked: {
                        timeSpinBox.increaseTimeUnit()
                    }

                    iconSrc: "../../images/arrow-up.png"
                }

                BottomMenuButton {
                    id: decreaseUnitLandscape
                    onClicked: {
                        timeSpinBox.decreaseTimeUnit()
                    }
                    visible: !isWindowWidthSmall

                    iconSrc: "../../images/arrow-down.png"
                }

                Rectangle {
                    id: separator
                    height: decreaseUnitLandscape.height*0.75
                    color: Num.sysPalette.shadow
                    width: 1
                    visible: !isWindowWidthSmall
                }

                BottomMenuButton {
                    onClicked: {
                        stackView.push(timePage)
                    }
                    visible: !isWindowWidthSmall

                    iconSrc: "../../images/appointment-new.png"
                }

                Rectangle {
                    id: separatorSearchLarge
                    height: decreaseUnitLandscape.height*0.75
                    color: Num.sysPalette.shadow
                    width: 1
                    visible: !isWindowWidthSmall
                }

                BottomMenuButton {
                    onClicked: {
                        stackView.push(findDialog)
                    }
                    visible: !isWindowWidthSmall

                    iconSrc: "../../images/edit-find.png"
                }

                BottomMenuButton {
                    id: telescopeUp
                    iconSrc: "../../images/telescope-up.png"
                    visible: bottomMenu.telescope
                    onPressed: {
                        ClientManagerLite.getTelescope().moveNS(TelescopeNS.MOTION_NORTH, TelescopeCommand.MOTION_START)
                    }
                    onClicked: {
                        ClientManagerLite.getTelescope().moveNS(TelescopeNS.MOTION_NORTH, TelescopeCommand.MOTION_STOP)
                    }
                }

                BottomMenuButton {
                    id: telescopeRight2
                    iconSrc: "../../images/telescope-right.png"
                    visible: bottomMenu.telescope && menuGrid.rows == 1
                    onPressed: {
                        ClientManagerLite.getTelescope().moveWE(TelescopeNS.MOTION_EAST, TelescopeCommand.MOTION_START)
                    }
                    onClicked: {
                        ClientManagerLite.getTelescope().moveWE(TelescopeNS.MOTION_EAST, TelescopeCommand.MOTION_STOP)
                    }
                }
            }
        }
    }
}
