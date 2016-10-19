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

AbstractButton {
    id: button
    property string iconSrc: ""
    width: icon.width + 5
    height: icon.height + 5

    Image {
        id: icon
        source: iconSrc
        width: sourceSize.width/num.pixelRatio
        height: sourceSize.height/num.pixelRatio
        anchors.centerIn: iconRect
    }

    background: Rectangle {
        id: iconRect
        radius: 5
        anchors {
            //centerIn: parent
            fill: parent
        }
        color: "black"
        border {
            color: num.sysPalette.light
            width: 1
        }
    }

    onDownChanged: {
        if(down) opacity = 0.6
        else opacity = 1
    }

    onPressed: {
        opacity = 0.6
    }

    onReleased: {
        opacity = 1
    }

    Behavior on opacity {
        OpacityAnimator { duration: 100 }
    }
}
