/***************************************************************************
                          SkyGuideSlide.qml  -  K Desktop Planetarium
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
    id: slide
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
        spacing: 10

        ObjHeader {}

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            color: "#ffffff"
            font.pixelSize: 24
            style: Text.Raised
            text: loader.modelData.slideTitle
        }

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumHeight: parent.width * 0.5
            Layout.maximumWidth: parent.width * 0.9
            fillMode: Image.PreserveAspectFit
            source: loader.modelData.slideImgPath
        }

        TextArea {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width * 0.9
            font.pixelSize: fontSizeText
            readOnly: true
            frameVisible: false
            backgroundVisible: false
            text: loader.modelData.slideText
            textColor: "#ffffff"
            horizontalAlignment: Text.AlignJustify
        }

        Item {
            Layout.preferredHeight: 40
        }
    }
}
