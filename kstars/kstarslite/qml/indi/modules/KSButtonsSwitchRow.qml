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
import QtQuick.Controls 1.4
import QtQuick.Window 2.2
import "../../constants" 1.0
import QtQuick.Layouts 1.2

Flow {
    id:buttonsRow
    property bool checkBox: false
    property string propName: ""
    property string deviceName: ""
    property bool exclusive: false
    Layout.fillWidth: true
    spacing: 10 * num.dp

    Connections {
        target: ClientManagerLite
        onNewINDISwitch: {
            if(buttonsRow.deviceName == deviceName) {
                if(buttonsRow.propName == propName) {
                    for(var i = 0; i < children.length; ++i) {
                        if(children[i].switchName == switchName) {
                            children[i].checked = isOn
                        }
                    }
                }
            }
        }
    }

    function addButton(propText, switchName, initChecked, enabled) {
        var buttonComp = Qt.createComponent("KSButtonSwitch.qml");
        var button = buttonComp.createObject(this)
        button.text = propText
        button.switchName = switchName
        button.parentRow = this
        button.checked = initChecked
        button.enabled = enabled
    }

    function addCheckBox(propText, switchName, initChecked, enabled) {
        var checkBoxComp = Qt.createComponent("KSCheckBox.qml");
        var checkBox = checkBoxComp.createObject(this)
        checkBox.text = propText
        checkBox.switchName = switchName
        checkBox.parentRow = this
        checkBox.checked = initChecked
        checkBox.enabled = enabled
    }

    function sendNewSwitch(switchName, button) {
        ClientManagerLite.sendNewINDISwitch(deviceName,propName,switchName)
        if(exclusive && button != null) {
            for(var i = 0; i < children.length; ++i) {
                if(children[i] !== button) {
                    children[i].checked = false
                }
            }
        }
    }
}

