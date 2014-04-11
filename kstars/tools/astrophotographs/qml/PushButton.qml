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
    id: pushButton
    property string text: "Button"
    property int vheight: text.height + 20
    property int vwidth: text.width + 20

    Rectangle {
        id: pushButtonRect
        /* adjust rectangle dimension based on text size */
        width: pushButton.vwidth
        height: pushButton.vheight

        // our border
        border.width: 2;
        border.color: "gray"
        radius: 4;
        smooth: true

        gradient: Gradient { // background gradient
            GradientStop { position: 0.0; color: "#424242" }
            GradientStop { position: 1.0; color: "black" }
        }

        Text {
            id: text // object id of this text
            color: "white"
            // center the text on parent
            anchors.horizontalCenter:parent.horizontalCenter;
            anchors.verticalCenter:parent.verticalCenter;
            text: pushButton.text
        }

        MouseArea {
                id: mouseRegion
                hoverEnabled: true
                anchors.fill: parent
                onEntered: pushButton.state = "buttonAreaEntered"
                onExited: pushButton.state = "buttonAreaExit"
            }

    }

    states: [
        State {
            name: "buttonAreaEntered"

            PropertyChanges {
                target: pushButtonRect
                opacity: 0.5
            }
        },
        State {
            name: "buttonAreaExit"

            PropertyChanges {
                target: pushButtonRect
                opacity: 1
            }
        }
    ]

}
