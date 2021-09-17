// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.2
import "../../constants" 1.0
import "../../modules"

Column {
    id: columnTextProp
    Layout.fillHeight: true
    Layout.fillWidth: true
    spacing: 5 * Num.dp
    property string propLabel: ""

    KSLabel {
        text: propLabel
    }

    Rectangle {
        id: separator
        height: Num.dp
        color: Num.sysPalette.light
        width: parent.width
    }

    function addField(isNumber, deviceName, propName, fieldName, propText, writable) {
        var textItem
        if(writable) {
                var textComp = Qt.createComponent("KSINDITextField.qml");
                textItem = textComp.createObject(this)
                textItem.deviceName = deviceName
                textItem.propName = propName
                textItem.fieldName = fieldName
                textItem.textField.text = propText
                textItem.isNumber = isNumber
        } else {
                textItem = Qt.createQmlObject('import QtQuick 2.6
                                        import QtQuick.Layouts 1.2
                                        import "../../constants" 1.0
                                        import "../../modules"
                                    KSText {
                                    }', this)
                textItem.text = propText
        }
        //textItem.anchors.top = separator.bottom
    }
}
