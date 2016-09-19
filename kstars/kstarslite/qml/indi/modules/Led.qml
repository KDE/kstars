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
    width: 16 * num.dp
    height: 16 * num.dp
    sourceSize.height: 16 * num.dp
    sourceSize.width: 16 * num.dp

    source: "images/" + num.density + "/grey.png"
    onColorChanged: {
        if(color == "red") {
            source = "images/" + num.density + "/red.png"
        } else if(color == "green") {
            source = "images/" + num.density + "/green.png"
        } else if(color == "yellow") {
            source = "images/" + num.density + "/yellow.png"
        } else if(color == "grey") {
            source = "images/" + num.density + "/grey.png"
        }
    }
}
