// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.7
import QtQuick.Controls 2.0
import "../../constants/" 1.0

MenuItem {
    id: menuItem
    onVisibleChanged: {
        //Height stays the same when visible is changed so we update height manually
        height = visible ? implicitHeight : 0
    }

    contentItem: Text {
        text: menuItem.text
        font: menuItem.font
        color: Num.sysPalette.text
        opacity: enabled ? 1.0 : 0.3
    }

    Rectangle {
        width: parent.width - 10
        height: 1
        color: Num.sysPalette.light
        anchors {
            bottom: parent.bottom
            bottomMargin: 5
            horizontalCenter: parent.horizontalCenter
        }
    }
}
