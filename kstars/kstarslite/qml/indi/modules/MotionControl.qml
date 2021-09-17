// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Window 2.2
import QtQuick.Layouts 1.2
import QtQuick.Controls 2.0
import TelescopeLiteEnums 1.0
import "../../constants" 1.0
import "../../modules"

ColumnLayout {
    id: motionCColumn
    anchors {
        fill: parent
        margins: Num.marginsKStab
    }
    spacing: 5 * Num.dp

    enabled: buttonsEnabled

    property string deviceName
    property var telescope: ClientManagerLite.getTelescope()
    property bool buttonsEnabled: ClientManagerLite.getTelescope().isConnected()

    Connections {
        target: ClientManagerLite
        onDeviceConnected: {
            buttonsEnabled = ClientManagerLite.getTelescope().isConnected()
        }
        onTelescopeAdded: {
            telescope = newTelescope
        }
    }

    //Row 1
    RowLayout {
        Layout.fillHeight: true
        anchors {
            left: parent.left
            right: parent.right
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeFocusOnTab: false


            onPressedChanged: {
                if(telescope) {
                    if(pressed) {
                        telescope.moveNS(TelescopeNS.MOTION_NORTH, TelescopeCommand.MOTION_START)
                        telescope.moveWE(TelescopeNS.MOTION_WEST, TelescopeCommand.MOTION_START)
                    } else {
                        telescope.moveNS(TelescopeNS.MOTION_NORTH, TelescopeCommand.MOTION_STOP)
                        telescope.moveWE(TelescopeNS.MOTION_WEST, TelescopeCommand.MOTION_STOP)
                    }
                }
            }

            text: xi18n("NW")
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeFocusOnTab: false


            onPressedChanged: {

                if(telescope) {
                    if(pressed) {
                        telescope.moveNS(TelescopeNS.MOTION_NORTH, TelescopeCommand.MOTION_START)
                    } else {
                        telescope.moveNS(TelescopeNS.MOTION_NORTH, TelescopeCommand.MOTION_STOP)
                    }
                }
            }

            text: xi18n("N")
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeFocusOnTab: false


            onPressedChanged: {

                if(telescope) {
                    if(pressed) {
                        telescope.moveNS(TelescopeNS.MOTION_NORTH, TelescopeCommand.MOTION_START)
                        telescope.moveWE(TelescopeNS.MOTION_EAST, TelescopeCommand.MOTION_START)
                    } else {
                        telescope.moveNS(TelescopeNS.MOTION_NORTH, TelescopeCommand.MOTION_STOP)
                        telescope.moveWE(TelescopeNS.MOTION_EAST, TelescopeCommand.MOTION_STOP)
                    }
                }
            }

            text: xi18n("NE")
        }
    }

    //Row 2
    RowLayout {
        Layout.fillHeight: true
        Layout.fillWidth: true

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeFocusOnTab: false


            onPressedChanged: {

                if(telescope) {
                    if(pressed) {
                        telescope.moveWE(TelescopeNS.MOTION_WEST, TelescopeCommand.MOTION_START)
                    } else {
                        telescope.moveWE(TelescopeNS.MOTION_WEST, TelescopeCommand.MOTION_STOP)
                    }
                }
            }

            text: xi18n("W")
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeFocusOnTab: false


            onPressedChanged: {

                if(telescope) {
                    telescope.abort();
                }
            }

            text: xi18n("Stop")
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeFocusOnTab: false


            onPressedChanged: {

                if(telescope) {
                    if(pressed) {
                        telescope.moveWE(TelescopeNS.MOTION_EAST, TelescopeCommand.MOTION_START)
                    } else {
                        telescope.moveWE(TelescopeNS.MOTION_EAST, TelescopeCommand.MOTION_STOP)
                    }
                }
            }

            text: xi18n("E")
        }
    }

    //Row 3
    RowLayout {
        Layout.fillHeight: true
        Layout.fillWidth: true

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeFocusOnTab: false
            onPressedChanged: {

                if(telescope) {
                    if(pressed) {
                        telescope.moveNS(TelescopeNS.MOTION_SOUTH, TelescopeCommand.MOTION_START)
                        telescope.moveWE(TelescopeNS.MOTION_WEST, TelescopeCommand.MOTION_START)
                    } else {
                        telescope.moveNS(TelescopeNS.MOTION_SOUTH, TelescopeCommand.MOTION_STOP)
                        telescope.moveWE(TelescopeNS.MOTION_WEST, TelescopeCommand.MOTION_STOP)
                    }
                }
            }

            text: xi18n("SW")
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeFocusOnTab: false


            text: xi18n("S")

            onPressedChanged: {

                if(telescope) {
                    if(pressed) {
                        telescope.moveNS(TelescopeNS.MOTION_SOUTH, TelescopeCommand.MOTION_START)
                    } else {
                        telescope.moveNS(TelescopeNS.MOTION_SOUTH, TelescopeCommand.MOTION_STOP)
                    }
                }
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeFocusOnTab: false


            onPressedChanged: {

                if(telescope) {
                    if(pressed) {
                        telescope.moveNS(TelescopeNS.MOTION_SOUTH, TelescopeCommand.MOTION_START)
                        telescope.moveWE(TelescopeNS.MOTION_EAST, TelescopeCommand.MOTION_START)
                    } else {
                        telescope.moveNS(TelescopeNS.MOTION_SOUTH, TelescopeCommand.MOTION_STOP)
                        telescope.moveWE(TelescopeNS.MOTION_EAST, TelescopeCommand.MOTION_STOP)
                    }
                }
            }

            text: xi18n("SE")
        }
    }

    //Slewing
    RowLayout {
        id: slewingRateRow
        Layout.fillHeight: true
        Layout.fillWidth: true

        Button {
            id: decreaseSlew
            height: motionCColumn.height * 0.15
            Layout.fillWidth: true
            activeFocusOnTab: false
            enabled: telescope.slewDecreasable

            Connections {
                target: telescope
                onSlewDecreasableChanged: {
                    decreaseSlew.enabled = telescope.slewDecreasable
                }
            }

            onClicked: {
                if(telescope) {
                    telescope.decreaseSlewRate()
                }
            }

            text: xi18n("-")
        }
        KSText {
            height: parent.height * 0.15
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter

            text: xi18n("Slew rate: " + telescope.slewRateLabel)
        }

        Button {
            id: increaseSlew
            height: motionCColumn.height * 0.15
            Layout.fillWidth: true
            activeFocusOnTab: false
            enabled: telescope.slewIncreasable

            Connections {
                target: telescope
                onSlewIncreasableChanged: {
                    increaseSlew.enabled = telescope.slewIncreasable
                }
            }

            onClicked: {
                if(telescope) {
                    telescope.increaseSlewRate()
                }
            }

            text: xi18n("+")
        }
    }
}
