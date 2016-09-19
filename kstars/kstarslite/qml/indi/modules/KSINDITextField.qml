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
import QtQuick.Layouts 1.2
import "../../constants" 1.0

import QtQuick 2.6
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.0

Flow {
    id: textRow
    spacing: 5 * num.dp
    anchors {
        left: parent.left
        right: parent.right
    }

    property Item textField: field
    property bool isNumber: true // false - text, true - number

    property string deviceName
    property string propName
    property string fieldName

    TextField {
        id: field
    }

    Button {
        text: xi18n("Set")
        onClicked: {
            if(isNumber) {
                ClientManagerLite.sendNewINDINumber(deviceName, propName, fieldName, field.text)
            } else {
                ClientManagerLite.sendNewINDIText(deviceName, propName, fieldName, field.text)
            }
            Qt.inputMethod.hide()
        }
    }

    Connections {
        target: ClientManagerLite
        onNewINDINumber: {
            if(isNumber) {
                if(textRow.deviceName == deviceName) {
                    if(textRow.propName == propName) {
                        if(textRow.fieldName == numberName) {
                            field.text = value
                        }
                    }
                }
            }
        }
        onNewINDIText: {
            if(!isNumber) {
                if(textRow.deviceName == deviceName) {
                    if(textRow.propName == propName) {
                        if(textRow.fieldName == fieldName) {
                            field.text = text
                        }
                    }
                }
            }
        }
    }
}
