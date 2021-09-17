// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Layouts 1.1

import QtQuick.Controls 2.1
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0
import "../constants" 1.0

Pane {
    property string title: ""
    visible: false

    background: Rectangle {
//        anchors.fill: parent
        color: Num.sysPalette.base
    }
}
