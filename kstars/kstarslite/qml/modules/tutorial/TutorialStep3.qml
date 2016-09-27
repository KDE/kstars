// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

import QtQuick 2.7

import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0

import QtQuick.Window 2.2 as Window
import QtQuick.Layouts 1.1

ColumnLayout {
    visible: step3
    anchors {
        left: parent.left
        right: parent.right
    }

    Image {
        rotation: 180
        source: "../../images/tutorial-arrow-vertical.png"
        anchors.horizontalCenter: parent.horizontalCenter
    }

    TutorialPane {
        anchors.horizontalCenter: parent.horizontalCenter

        contentWidth: parent.width * 0.75
        title: xi18n("Top Menu")
        description: xi18n("By tapping on this arrow you can access top menu from which you can control visibility of different sky objects")

        onNextClicked: {
            step3 = false
            step4 = true
        }
    }
}
