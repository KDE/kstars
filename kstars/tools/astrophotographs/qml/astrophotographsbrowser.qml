// Copyright (C) 2014 Vijay Dhameliya <vijay.atwork13@gmail.com>
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

import QtQuick 1.1

Rectangle {
    id: container
    objectName: "containerObj"
    width: 376
    height: 575
    color: "#020518"
    anchors.fill: parent

    Text {
        id: title
        x: 9
        y: 34
        width: 209
        height: 46
        color: "#59ad0e"
        text: i18n("Astrophotographs Browser")
        verticalAlignment: Text.AlignVCenter
        font.family: "Cantarell"
        font.bold: false
        font.pixelSize: 22
    }
}
