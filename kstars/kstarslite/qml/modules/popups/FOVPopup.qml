// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

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
