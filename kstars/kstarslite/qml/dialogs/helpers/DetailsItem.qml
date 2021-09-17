// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.7
import QtQuick.Controls 2.0
import "../../modules"

Column {
    property string label
    property string value
    spacing: 5
    visible: value.length

    width: parent.width

    KSLabel {
        text: label
        font.pointSize: 13
    }

    Rectangle {
        width: parent.width
        height: 1
        color: "grey"
    }

    KSLabel {
        font.pointSize: 11
        text: value
    }
}
