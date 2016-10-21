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
import "../../modules"

Column {
    property string label
    property string value
    spacing: 5
    visible: value.length

    width: parent.width

    KSLabel {
        text: label
        font.pointSize: 13
    }

    Rectangle {
        width: parent.width
        height: 1
        color: "grey"
    }

    KSLabel {
        font.pointSize: 11
        text: value
    }
}
