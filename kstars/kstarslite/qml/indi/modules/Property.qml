// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Window 2.2
import QtQuick.Layouts 1.2
import QtQuick.Controls 2.0
import "../../constants"
import TelescopeLiteEnums 1.0

ColumnLayout {
    id: columnProp

    Layout.fillHeight: true
    width: parentTab == null ? 0 : parentTab.width

    property string deviceName: ""
    property string propName: ""
    property string label: ""
    property Item parentTab
    property ComboBox comboBox: null
    property Flow buttonRow: null

    KSLed {
        id: led
        deviceName: columnProp.deviceName
        propName: columnProp.propName
        label: columnProp.label

        Component.onCompleted: {
            syncLEDProperty()
        }
    }

    Rectangle {
        id: separator
        height: Num.dp
        color: "grey"
        Layout.fillWidth: true
    }

    Connections {
        target: ClientManagerLite
        onNewLEDState: {
            if(columnProp.deviceName == deviceName) {
                if(columnProp.propName == propName) {
                    led.syncLEDProperty()
                }
            }
        }
        onCreateINDIButton: {
            if(columnProp.deviceName == deviceName) {
                if(columnProp.propName == propName) {
                    if(buttonRow == null) {
                        var buttonRowComp = Qt.createComponent("KSButtonsSwitchRow.qml");
                        buttonRow = buttonRowComp.createObject(columnProp)
                        buttonRow.deviceName = deviceName
                        buttonRow.propName = propName
                        buttonRow.exclusive = exclusive
                        buttonRow.width = Qt.binding(function() { return parentTab.width })
                    }
                    buttonRow.addButton(propText, switchName, checked, enabled)
                }
            }
        }
        onCreateINDIRadio: {
            if(columnProp.deviceName == deviceName) {
                if(columnProp.propName == propName) {
                    if(buttonRow == null) {
                        var buttonRowComp = Qt.createComponent("KSButtonsSwitchRow.qml");
                        buttonRow = buttonRowComp.createObject(columnProp)
                        buttonRow.deviceName = deviceName
                        buttonRow.propName = propName
                        buttonRow.exclusive = exclusive
                        buttonRow.checkBox = true
                        buttonRow.width = Qt.binding(function() { return parentTab.width })
                    }
                    buttonRow.addCheckBox(propText, switchName, checked, enabled)
                }
            }
        }
        onCreateINDIMenu: {
            if(columnProp.deviceName == deviceName) {
                if(columnProp.propName == propName) {
                    if(comboBox == null) {
                        var CBoxComponent = Qt.createComponent("KSComboBox.qml");
                        comboBox = CBoxComponent.createObject(columnProp)
                        comboBox.deviceName = deviceName
                        comboBox.propName = propName
                        comboBox.textRole = "text"
                    }
                    comboBox.model.append({"text": switchLabel, "name": switchName})
                    if(isSelected) {
                        comboBox.currentIndex = comboBox.model.length - 1
                    }
                }
            }
        }

        onCreateINDIText: {
            if(columnProp.deviceName == deviceName) {
                if(columnProp.propName == propName) {
                    var indiTextComp = Qt.createComponent("KSINDIText.qml");
                    var indiText = indiTextComp.createObject(columnProp)
                    indiText.addField(false, deviceName, propName, fieldName, propText, write)
                    indiText.propLabel = propLabel
                    //indiText.width = Qt.binding(function() { return parentTab.width })
                }
            }
        }
        onCreateINDINumber: {
            if(columnProp.deviceName == deviceName) {
                if(columnProp.propName == propName) {
                    var indiNumComp = Qt.createComponent("KSINDIText.qml");
                    var indiNum = indiNumComp.createObject(columnProp)
                    indiNum.addField(true, deviceName, propName, numberName, propText, write)
                    indiNum.propLabel = propLabel
                }
            }
        }
        onCreateINDILight: {
            if(columnProp.deviceName == deviceName) {
                if(columnProp.propName == propName) {
                    var lightComp = Qt.createComponent("KSLed.qml")
                    var light = lightComp.createObject(columnProp)
                    light.deviceName = deviceName
                    light.propName = propName
                    light.label = label
                    light.name = name
                    light.syncLEDLight()
                }
            }
        }
    }
}
