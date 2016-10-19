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
import "../../modules"

Pane {
    property string title
    property string description
    signal nextClicked()
    focus: true
    Column {
        width: parent.width
        height: childrenRect.height

        KSLabel {
            width: parent.width
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: title
            font.pointSize: 20
        }

        KSText {
            width: parent.width
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: description
        }

        Flow {
            property int buttonWidth: children[0].width + children[1].width + spacing * 2
            width: parent.width > buttonWidth ? buttonWidth : parent.width
            spacing: 5
            anchors.horizontalCenter: parent.horizontalCenter

            Button {
                text: xi18n("Exit")
                onClicked: askExitTutorial()
            }

            Button {
                text: xi18n("Next")
                onClicked: {
                    nextClicked()
                }
            }
        }
    }
}

