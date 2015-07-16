/***************************************************************************
                          SkyGuideInfo.qml  -  K Desktop Planetarium
                             -------------------
    begin                : 2015/07/01
    copyright            : (C) 2015 by Marcos Cardinot
    email                : mcardinot@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

ColumnLayout {
    id: info
    spacing: 10
    Layout.alignment: Qt.AlignHCenter

    ObjHeader {}

    ObjRectangle {
        TextArea {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height - frameHMargin
            width: parent.width - frameVMargin
            frameVisible: false
            font.pixelSize: fontSizeText
            readOnly: true
            text: loader.modelData.description
        }
    }

    ObjRectangle {
        Layout.maximumHeight: 100

        GridLayout {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height - frameHMargin
            width: parent.width - frameVMargin
            columns: 2

            Text { text: "Creation Date:"; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.creationDate; font.pixelSize: fontSizeText; }

            Text { text: "Language:"; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.language; font.pixelSize: fontSizeText; }

            Text { text: "Version:"; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.version; font.pixelSize: fontSizeText; }

            Text { text: "Slides:"; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.contents.length; font.pixelSize: fontSizeText; }
        }
    }

    ObjRectangle {
        Component {
            id: contentsDelegate
            Item {
                width: contentsView.width
                height: 25
                Text {
                    text: title
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: contentsView.currentIndex = index
                    onDoubleClicked: {
                        loader.modelData.currentSlide = contentsView.currentIndex;
                        goToPage({'name': 'SLIDE', 'modelData': loader.modelData});
                    }
                }
            }
        }

        ListView {
            id: contentsView
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height - frameHMargin
            width: parent.width - frameVMargin
            focus: true
            model: ListModel {}
            highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
            delegate: contentsDelegate
            Keys.onReturnPressed: {
                loader.modelData.currentSlide = contentsView.currentIndex;
                goToPage({'name': 'SLIDE', 'modelData': loader.modelData});
            }
            Component.onCompleted: {
                var s = loader.modelData.contents;
                for (var key in s) {
                    model.append({"title": s[key]});
                }
            }
        }
    }

    Item {
        Layout.fillHeight: true;
    }
}
