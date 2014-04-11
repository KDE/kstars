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
import "../../whatsinteresting/qml/"

Rectangle {
    id: container
    objectName: "containerObj"
    width: 375
    height: 575
    color: "#020518"
    anchors.fill: parent

    Text {
        id: title
        x: 10
        y: 35
        width: 210
        height: 45
        color: "#59ad0e"
        text: i18n("Astrophotographs Browser")
        verticalAlignment: Text.AlignVCenter
        font.family: "Cantarell"
        font.bold: false
        font.pixelSize: 22
    }

    Rectangle {
        id: searchContainer
        y: 90
        width: parent.width - 100
        height: 40
        color: "transparent"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20

        Rectangle {
            id: searchTextContainer
            width: parent.width - searchButton.vwidth - 10
            height: parent.height
            color: "#00060b"
            radius: 10
            opacity: 0.500
            border.width: 4
            border.color: "black"

            Rectangle {
                width: parent.width
                height: parent.height
                color: "transparent"
                anchors.top: parent.top
                anchors.topMargin: 10
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 10
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 10

                TextInput {
                    id: searchBar
                    width: parent.width
                    height: parent.height
                    color: "#59ad0e"
                    text: "Quick Search..."
                    font.pixelSize: 18
                }
            }

        }

        PushButton {
            id: searchButton
            vheight: parent.height - 4
            text: "Search"
            anchors.left: searchTextContainer.right
            anchors.leftMargin: 10
            anchors.top: parent.top
            anchors.bottom: parent.bottom
        }
    }

    Rectangle {
        id: dataContainer
        width: parent.width
        height: parent.height
        color: "#00060b"
        radius: 10
        opacity: 0.500
        border.width: 4
        border.color: "black"
        anchors.top: searchContainer.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20

        ListModel {
             id: fruitModel

             ListElement {
                 path: "/home/vijay13/Pictures/1.jpg"
                 name: "Sky Object One"
                 date: "14th April 2014"
             }

             ListElement {
                 path: "/home/vijay13/Pictures/2.jpg"
                 name: "Sky Object Two"
                 date: "15th April 2014"
             }

             ListElement {
                 path: "/home/vijay13/Pictures/3.jpg"
                 name: "Sky Object Three"
                 date: "16th April 2014"
             }

             ListElement {
                 path: "/home/vijay13/Pictures/4.jpg"
                 name: "Sky Object Four"
                 date: "16th April 2014"
             }

             ListElement {
                 path: "/home/vijay13/Pictures/2.jpg"
                 name: "Sky Object Five"
                 date: "16th April 2014"
             }

         }

        Component {
                 id: fruitDelegate
                 Row {
                     spacing: 10
                     AstrophotographItem {
                         id: testItem
                         imagePath: path
                         imageName: name
                         imageDate: date
                         width: dataContainer.width - 30
                         height: 100
                     }
                 }
             }

        ListView {
             id: resultListView
             model: fruitModel
             delegate: fruitDelegate
             anchors.fill: parent
             clip: true

             ScrollBar {
                 flickable: resultListView
             }
        }

    }

}
