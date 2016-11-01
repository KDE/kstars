// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

import QtQuick.Controls 2.0
import QtQuick 2.7
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../../modules"
import QtPositioning 5.2

Popup {
    x: (window.width - width) / 2
    y: window.height / 6
    focus: true
    modal: true

    background: Rectangle {
        anchors.fill: parent
        color: num.sysPalette.base
    }

    ColumnLayout {
        id: aboutDialog
        focus: true
        width: Math.min(window.width, window.height) / 3 * 2

        Column {
            id: loadingColumn
            width: parent.width

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
            }

            KSLabel {
                id: fetchText
                width: parent.width
                wrapMode: Label.Wrap
                horizontalAlignment: Label.AlignHCenter
                text: locationEdit.loadingText
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: xi18n("Cancel")
                onClicked: {
                    close()
                }
            }
        }
    }
}
