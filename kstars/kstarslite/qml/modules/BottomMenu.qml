import QtQuick 2.7
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0

import QtQuick.Window 2.2
import "../constants" 1.0
import "helpers"

ColumnLayout {
    id: bottomMenu
    property int padding: 10

    property double openOffset: bottomMenu.height - bottomBar.background.radius //Hide bottom round corners
    property double closedOffset: arrowUp.height + padding
    property string prevState

    property bool isWindowWidthSmall: window.width < menuGrid.maxWidth

    //Hide on slew
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
            anchors.fill: parent
            onPressed: {
                bottomMenu.state = bottomMenu.state == "closed" ? "open" : "closed"
            }
        }

        Behavior on rotation {
            RotationAnimation {
                duration: 200; direction: RotationAnimation.Counterclockwise
            }
        }
    }

    Pane {
        id: bottomBar
        anchors.horizontalCenter: parent.horizontalCenter

        background: Rectangle {
            id: menuRect
            color: num.sysPalette.dark
            border {
                width: 1
                color: num.sysPalette.light
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

                BottomMenuButton {
                    onClicked: {
                        stackView.push(timePage)
                    }
                    visible: isWindowWidthSmall
                    anchors.right: parent.right

                    iconSrc: "../../images/appointment-new.png"
                }
            }

            RowLayout {
                id: secondRow
                Layout.fillHeight: true
                Layout.fillWidth: true

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
                    id: separatorBar
                    height: decreaseUnitLandscape.height*0.75
                    color: num.sysPalette.light
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
            }
        }
    }
}
