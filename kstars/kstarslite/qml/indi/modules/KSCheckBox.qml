// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Controls 2.0

CheckBox {
    property string switchName: ""
    property Flow parentRow

    onClicked: {
        parentRow.sendNewSwitch(switchName, null)
    }
}
