/***************************************************************************
                          ObjHeader.qml  -  K Desktop Planetarium
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
import QtQuick.Layouts 1.1

Rectangle {
    Layout.alignment: Qt.AlignTop
    Layout.fillWidth: true
    Layout.preferredHeight: 50
    gradient: Gradient {
        GradientStop {
            position: 0.344
            color: "#000b5f"
        }
        GradientStop {
            position: 1
            color: "#0319ca"
        }
    }

    ColumnLayout {
        Text {
            Layout.alignment: Qt.AlignLeft
            Layout.fillWidth: true
            color: "#ffffff"
            font.pixelSize: 24
            style: Text.Raised
            text: loader.modelData.title
        }

        RowLayout {
            id: pathMenu
            property string color: "#2a8af5"
            property string colorHover: "#80b5f1"
            property int fontSize: 12

            Text {
                id: homePath
                color: pathMenu.color
                font.pixelSize: pathMenu.fontSize
                text: "SkyGuide"
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: homePath.color = pathMenu.colorHover
                    onExited: homePath.color = pathMenu.color
                    onClicked: goToPage({'name': 'HOME', 'modelData': null, 'slide': -1})
                }
            }

            Text {
                id: infoPath
                color: pathMenu.color
                font.pixelSize: pathMenu.fontSize
                text: "> " + loader.modelData.title
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: infoPath.color = pathMenu.colorHover
                    onExited: infoPath.color = pathMenu.color
                    onClicked: {
                        goToPage({'name': 'INFO', 'modelData': loader.modelData, 'slide': -1});
                    }
                }
            }

            Text {
                id: slidePath
                color: pathMenu.color
                font.pixelSize: pathMenu.fontSize
                text: loader.modelData.slideTitle ? "> " + loader.modelData.slideTitle : ""
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: slidePath.color = pathMenu.colorHover
                    onExited: slidePath.color = pathMenu.color
                    onClicked: goToPage({'name': 'SLIDE',
                                         'modelData': loader.modelData,
                                         'slide': loader.modelData.currentSlide})
                }
            }
        }
    }
}
