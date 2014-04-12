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
    id: iconPushButton
    property string text: "Button"
    property string iconPath: ""
    property int vheight: text.height + 10
    property int vwidth: text.width + 20

    Rectangle {
        id: iconPushButtonRect
        width: iconPushButton.vwidth
        height: iconPushButton.vheight
        border.width: 2;
        border.color: "gray"
        radius: 4;
        smooth: true

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#424242" }
            GradientStop { position: 1.0; color: "black" }
        }

        Rectangle {
            id: iconHolderRect
            width: iconPushButtonRect.width / 2.75
            height: iconPushButtonRect.height - 4
            anchors.left: iconPushButtonRect.left
            anchors.leftMargin: 5
            anchors.verticalCenter: iconPushButtonRect.verticalCenter
            color: "transparent"

            Image {
                opacity: 1.0
                anchors.fill: parent
                smooth: true
                source: iconPushButton.iconPath
            }

        }

        Text {
            id: text
            color: "white"
            font.pixelSize: 14
            anchors.left: iconHolderRect.right
            anchors.leftMargin: 5
            anchors.verticalCenter: iconPushButtonRect.verticalCenter
            text: iconPushButton.text
        }

        MouseArea {
            id: mouseRegion
            hoverEnabled: true
            anchors.fill: parent
            onEntered: iconPushButton.state = "buttonAreaEntered"
            onExited: iconPushButton.state = "buttonAreaExit"
        }

    }

    states: [
        State {
            name: "buttonAreaEntered"

            PropertyChanges {
                target: iconPushButtonRect
                opacity: 0.5
            }
        },
        State {
            name: "buttonAreaExit"

            PropertyChanges {
                target: iconPushButtonRect
                opacity: 1
            }
        }
    ]

}
