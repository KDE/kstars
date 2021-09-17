// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Layouts 1.2
import QtQuick.Controls 2.0

import "../../constants" 1.0
import "../../modules"


Flow {
    id: textRow
    spacing: 5 * Num.dp
    anchors {
        left: parent.left
        right: parent.right
    }

    property Item textField: field
    property bool isNumber: true // false - text, true - number

    property string deviceName
    property string propName
    property string fieldName

    KSTextField {
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
