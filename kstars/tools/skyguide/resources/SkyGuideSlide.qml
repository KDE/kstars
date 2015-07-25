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

ColumnLayout {
    id: slide
    spacing: 10
    Layout.alignment: Qt.AlignHCenter

    ObjHeader {}

    Text {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        color: "#1703ca"
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
        frameVisible: false
        font.pixelSize: fontSizeText
        readOnly: true
        text: loader.modelData.slideText
    }
}
