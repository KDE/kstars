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
            Text {
                id: slidepath
                color: "#80b5f1"
                font.pixelSize: 12
                text: "SkyGuide"
            }

            Text {
                color: slidepath.color
                font: slidepath.font
                text: "> " + loader.modelData.title
            }

            Text {
                color: slidepath.color
                font: slidepath.font
                text: "> " + loader.modelData.slideTitle
            }
        }
    }
}
