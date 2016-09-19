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
    onVisibleChanged: {
        //Height stays the same when visible is changed so we update height manually
        height = visible ? implicitHeight : 0
    }

    Rectangle {
        width: parent.width - 10
        height: 1
        color: "#E8E8E8"
        anchors {
            bottom: parent.bottom
            bottomMargin: 5
            horizontalCenter: parent.horizontalCenter
        }
    }
}
