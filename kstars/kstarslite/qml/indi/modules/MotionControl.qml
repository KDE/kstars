// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

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
        topMargin: 5 * num.dp
        bottomMargin: 5 * num.dp
    }
    spacing: 5 * num.dp

    enabled: buttonsEnabled

    property string deviceName
    property var telescope: ClientManagerLite.getTelescope(deviceName)
    property bool buttonsEnabled: telescope.isConnected()

    Connections {
        target: ClientManagerLite
        onDeviceConnected: {
            if(motionCColumn.deviceName == deviceName) {
                buttonsEnabled = isConnected
            }
        }
        onTelescopeAdded: {
            if(newTelescope.getDeviceName() === motionCColumn.deviceName) {
                telescope = newTelescope
            }
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
