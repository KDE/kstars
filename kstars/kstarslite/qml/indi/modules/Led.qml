// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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
