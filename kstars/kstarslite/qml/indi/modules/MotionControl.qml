import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.2
import QtQuick.Controls 1.4
import org.kde.kirigami 1.0 as Kirigami
import TelescopeLiteEnums 1.0
import "../../constants" 1.0

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
            if(newTelescope.getDeviceName() == motionCColumn.deviceName) {
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

            text: "NW"
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

            text: "N"
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

            text: "NE"
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

            text: "W"
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

            text: "Stop"
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

            text: "E"
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

            text: "SW"
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeFocusOnTab: false


            text: "S"

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

            text: "SE"
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

            text: "-"
        }
        Text {
            height: parent.height * 0.15
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter

            text: "Slewing speed control"
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

            text: "+"
        }
    }
}
