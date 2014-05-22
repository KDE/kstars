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

Item {
    id: detailListItem
    height: detailText.height + 2
    property string itemText: ""

    Text {
        id: detailText
        width: detailListItem.width
        horizontalAlignment: Text.AlignLeft
        color: "white"
        text: itemText
        font.pixelSize: 13
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere

        MouseArea {
            id: textRegion
            hoverEnabled: true
            anchors.fill: parent
            onEntered: detailListItem.state = "textAreaEntered"
            onExited: detailListItem.state = "textAreaExit"
        }
    }

    states: [
        State {
            name: "textAreaEntered"

            PropertyChanges {
                target: detailText
                color: "yellow"
                font.pixelSize: 14
            }
        },
        State {
            name: "textAreaExit"

            PropertyChanges {
                target: detailText
                color: "white"
                font.pixelSize: 13
            }
        }

    ]

}
