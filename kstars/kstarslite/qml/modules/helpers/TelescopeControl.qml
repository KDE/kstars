// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

ColumnLayout {
    property var telescope: null
    anchors.horizontalCenter: parent.horizontalCenter

    Label {
        font {
            pointSize: 13
        }
        text: telescope == null ? "" : telescope.deviceName
    }

    RowLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        Button {
            text: xi18n("Slew")
            onClicked: {
                telescope.slew(SkyMapLite.clickedObjectLite)
            }
        }
        Button {
            text: xi18n("Sync")
            onClicked: {
                telescope.sync(SkyMapLite.clickedObjectLite)
            }
        }
    }
}
