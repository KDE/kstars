// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick.Controls 2.0
import QtQuick 2.7
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../../modules/"
import "../../modules/helpers"

Menu {
    id: locationsMenu
    modal: true
    transformOrigin: Menu.Center
    padding: 0
    property string locName
    property bool isReadOnly
    background: Rectangle {
        implicitWidth: 200
        color: Num.sysPalette.base
        radius: 5
    }

    function openMenu(name) {
        locName = name
        isReadOnly = LocationDialogLite.isReadOnly(name)

        open()
    }

    Column {
        width: parent.width
        spacing: 10

        KSLabel {
            id: name
            text: locName
            wrapMode: Label.WrapAtWordBoundaryOrAnywhere
            width: parent.width
            font.pointSize: 12
            anchors {
                left: parent.left
                leftMargin: 10
            }
        }

        Rectangle {
            color: "grey"
            width: parent.width - 10
            height: 1
            anchors {
                horizontalCenter: parent.horizontalCenter
            }
        }
    }

    KSMenuItem {
        text: xi18n("Set as my location")
        onTriggered: {
            if(LocationDialogLite.setLocation(locName)) {
                skyMapLite.notification.showNotification(xi18n("Set %1 as the current location", locName))
            } else {
                skyMapLite.notification.showNotification(xi18n("Could not set as the current location", locName))
            }
            locationDialog.filterCities()
        }
    }

    KSMenuItem {
        text: isReadOnly ? xi18n("View") : xi18n("Edit")
        onTriggered: {
            if(isReadOnly) {
                locationEdit.openEdit(locName, true)
            } else {
                locationEdit.openEdit(locName, false)
            }
        }
    }

    KSMenuItem {
        enabled: !isReadOnly
        text: xi18n("Delete")
        onTriggered: {
            LocationDialogLite.deleteCity(locName)
            skyMapLite.notification.showNotification(xi18n("Deleted location %1", locName))
            locationDialog.filterCities()
        }
    }
}
