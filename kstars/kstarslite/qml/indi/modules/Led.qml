// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

import QtQuick 2.6
import "../../constants" 1.0

Image {
    property string color: "grey"
    width: 16 * Num.dp
    height: 16 * Num.dp
    sourceSize.height: 16 * Num.dp
    sourceSize.width: 16 * Num.dp

    source: "images/" + Num.density + "/grey.png"
    onColorChanged: {
        if(color == "red") {
            source = "images/" + Num.density + "/red.png"
        } else if(color == "green") {
            source = "images/" + Num.density + "/green.png"
        } else if(color == "yellow") {
            source = "images/" + Num.density + "/yellow.png"
        } else if(color == "grey") {
            source = "images/" + Num.density + "/grey.png"
        }
    }
}
