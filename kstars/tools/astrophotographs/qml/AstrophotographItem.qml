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
    id: astrophotographItem
    property string imagePath: ""
    property string imageName: "Sky Object Name"
    property string imageDate: "13 April 2014"

    Rectangle {
        id: imageItemContainer
        width: astrophotographItem.width
        height: astrophotographItem.height
        color: "#00060b"
        radius: 10
        opacity: 1
        border.width: 2
        border.color: "white"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 10
        anchors.leftMargin: 15

        Rectangle {
            id: imgPathRect
            width: imageItemContainer.width / 3
            height: imageItemContainer.height
            anchors.left: imageItemContainer.left

            Rectangle {
                id: imgContainer
                anchors.left: imgPathRect.left
                anchors.right: imgPathRect.right
                anchors.top: imgPathRect.top
                anchors.bottom: imgPathRect.bottom
                anchors.margins: 5

                Image {
                    opacity: 1
                    anchors.fill: parent
                    smooth: true
                    source: astrophotographItem.imagePath
                 }

            }
        }


        Rectangle {
            id: imgNameRect
            anchors.left: imgPathRect.right
            anchors.top: imageItemContainer.top
            anchors.bottom: imageItemContainer.verticalCenter
            anchors.margins: 5

            Text {
                id: imgName
                color: "white"
                font.pixelSize: 20
                text: astrophotographItem.imageName
            }
        }

        Rectangle {
            id: imgDateRect
            anchors.top: imgNameRect.bottom
            anchors.left: imgPathRect.right
            anchors.margins: 5

            Text {
                id: imgDate
                color: "white"
                font.pixelSize: 16
                text: astrophotographItem.imageDate
            }
        }

    }

}
