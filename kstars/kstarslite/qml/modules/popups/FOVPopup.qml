// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick.Controls 2.0
import QtQuick 2.6
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../../modules"
import KStarsLiteEnums 1.0

Popup {
    id: fovPopup
    focus: true
    modal: true
    width: fovList.implicitWidth
    height: parent.height > fovList.implicitHeight ? fovList.implicitHeight : parent.height
    background: Rectangle {
        anchors.fill: parent
        color: Num.sysPalette.base
    }

    KSListView {
        id: fovList
        anchors {
            fill: parent
            centerIn: parent
        }
        checkable: true

        model: SkyMapLite.FOVSymbols
        onClicked: {
            SkyMapLite.setFOVVisible(index, checked)
        }
    }
}
