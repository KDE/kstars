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
import "../../constants/" 1.0

MenuItem {
    id: menuItem
    onVisibleChanged: {
        //Height stays the same when visible is changed so we update height manually
        height = visible ? implicitHeight : 0
    }

    contentItem: Text {
        text: menuItem.text
        font: menuItem.font
        color: num.sysPalette.text
        opacity: enabled ? 1.0 : 0.3
    }

    Rectangle {
        width: parent.width - 10
        height: 1
        color: num.sysPalette.light
        anchors {
            bottom: parent.bottom
            bottomMargin: 5
            horizontalCenter: parent.horizontalCenter
        }
    }
}
