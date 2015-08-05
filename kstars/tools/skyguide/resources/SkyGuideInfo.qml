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

Rectangle {
    id: info
    gradient: Gradient {
        GradientStop {
            position: 0.120
            color: "#120f16"
        }

        GradientStop {
            position: 0.220
            color: "#28262f"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        Layout.alignment: Qt.AlignHCenter

        ObjHeader {
            id: header
        }

        Text {
            id: description
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.9
            anchors.top: header.bottom
            anchors.topMargin: 5
            font.pixelSize: fontSizeText
            text: loader.modelData.description
            wrapMode: Text.WordWrap
            color: "#A3D3F2"
        }

        GridLayout {
            id: grid
            columns: 2
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.9
            anchors.top: description.bottom
            anchors.topMargin: 15
            property string textcolor: "#ffffff"

            Text { text: "Creation Date:"; color: grid.textcolor; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.creationDateStr; color: grid.textcolor; font.pixelSize: fontSizeText; }

            Text { text: "Language:"; color: grid.textcolor; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.language; color: grid.textcolor; font.pixelSize: fontSizeText; }

            Text { text: "Version:"; color: grid.textcolor; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.version; color: grid.textcolor; font.pixelSize: fontSizeText; }

            Text { text: "Slides:"; color: grid.textcolor; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.contents.length; color: grid.textcolor; font.pixelSize: fontSizeText; }
        }

        ListView {
            id: contentsView
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true;
            Layout.preferredWidth: parent.width * 0.9
            anchors.top: grid.bottom
            anchors.topMargin: 15
            spacing: 7
            focus: true
            clip: true
            model: ListModel {}
            delegate: contentsDelegate
            Keys.onReturnPressed: goToPage(getPageObj('SLIDE', loader.modelData,
                                                      contentsView.currentIndex))
            Component.onCompleted: {
                var s = loader.modelData.contents;
                for (var key in s) {
                    model.append({"title": s[key]});
                }
            }
        }

        Component {
            id: contentsDelegate
            Rectangle {
                width: contentsView.width
                height: 25
                color: ListView.isCurrentItem ? "#157efb" : "#39475C"
                radius: 5

                Text {
                    anchors.fill: parent
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    verticalAlignment: Text.AlignVCenter
                    color: "#ffffff"
                    text: title
                    style: Text.Sunken
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: contentsView.currentIndex = index
                    onDoubleClicked: goToPage(getPageObj('SLIDE', loader.modelData,
                                                         contentsView.currentIndex))
                }
            }
        }

        Item {
            Layout.preferredHeight: 40
        }
    }
}
