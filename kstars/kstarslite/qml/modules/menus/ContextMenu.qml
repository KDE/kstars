// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick.Controls 2.0
import QtQuick 2.7
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../helpers"
import "../../modules"

Menu {
    id: contextMenu
    modal: true
    transformOrigin: Menu.Center
    padding: 5
    background: Rectangle {
        implicitWidth: 200
        color: Num.sysPalette.base
        radius: 5
    }

    property bool isPoint: false

    function openPoint() {
        isPoint = true
        open()
    }

    function openObject() {
        isPoint = false
        open()
    }

    Column {
        width: parent.width
        spacing: 10

        KSLabel {
            id: objectName
            text: isPoint ? xi18n("Empty Sky") : SkyMapLite.clickedObjectLite.translatedName
            wrapMode: Label.WrapAtWordBoundaryOrAnywhere
            width: parent.width
            font.pointSize: 12
            anchors {
                left: parent.left
                leftMargin: 10
            }
        }

        Rectangle {
            color: Num.sysPalette.light
            width: parent.width - 10
            height: 1
            anchors {
                horizontalCenter: parent.horizontalCenter
            }
        }
    }

    KSMenuItem {
        text: xi18n("Center and Track")
        onTriggered: {
            contextMenu.close()
            SkyMapLite.slotCenter()
            SkyMapLite.centerLocked = true
        }
    }

    KSMenuItem {
        visible: !isPoint
        text: xi18n("Details")
        onTriggered: stackView.push(detailsDialog)
    }

    Item {
        id: hSpacer
        height: telescopeCol.isTelescope ? 15 : 0
    }

    ColumnLayout {
        id: telescopeCol
        width: parent.width
        spacing: 10
        /*If we don't set height to 0 when telescope is not connected there will be some blank space in the bottom
        of menu*/
        height: isTelescope ? implicitHeight : 0

        property bool isTelescope: telescope == null ? false : true
        property var telescope: null

        Connections {
            target: ClientManagerLite

            onTelescopeAdded: {
                if(!telescopeCol.isTelescope) {
                    telescopeCol.telescope = newTelescope
                    telescopeName.text = newTelescope.deviceName
                }
            }

            onTelescopeRemoved: {
                telescopeCol.telescope = null
            }
        }

        KSText {
            id: telescopeName
            visible: telescopeCol.isTelescope
            wrapMode: Text.Wrap
            font.pointSize: 12
            anchors {
                left: parent.left
                leftMargin: 10
            }
        }

        Rectangle {
            color: Num.sysPalette.light
            visible: telescopeCol.isTelescope
            width: parent.width - 10
            height: 1
            anchors {
                horizontalCenter: parent.horizontalCenter
            }
        }
    }

    KSMenuItem {
        visible: telescopeCol.isTelescope
        text: xi18n("Slew")
        onTriggered: telescopeCol.telescope.slew(SkyMapLite.clickedObjectLite)
    }

    KSMenuItem {
        visible: telescopeCol.isTelescope
        text: xi18n("Sync")
        onTriggered: telescopeCol.telescope.sync(SkyMapLite.clickedObjectLite)
    }
}
