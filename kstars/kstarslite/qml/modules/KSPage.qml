// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

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
        color: num.sysPalette.base
    }
}
