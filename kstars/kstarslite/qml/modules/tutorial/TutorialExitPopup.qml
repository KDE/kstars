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

Popup {
    id: tutorialExit
    focus: true
    modal: true

    ColumnLayout {
        id: studyCol
        width: parent.width
        height: childrenRect.height

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: xi18n("Are you sure you want to exit tutorial?")
        }

        RowLayout {
            anchors.horizontalCenter: parent.horizontalCenter

            Button {
                text: xi18n("Cancel")
                onClicked: close()
            }

            Button {
                text: xi18n("Yes")
                onClicked: {
                    exitTutorial()
                    close()
                }
            }
        }
    }
}
