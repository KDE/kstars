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
import QtQuick.Controls 2.0

Button {
    property string switchName: ""
    property Flow parentRow
    checkable: true
    checked: true

    onClicked: {
        parentRow.sendNewSwitch(switchName, null)
        Qt.inputMethod.hide()
    }
}
