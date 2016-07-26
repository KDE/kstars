import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.2
import QtQuick.Controls 1.4
import "../../constants"
import org.kde.kirigami 1.0 as Kirigami
import TelescopeLiteEnums 1.0

ColumnLayout {
    id: columnProp

    Layout.fillHeight: true
    Layout.fillWidth: true

    property string name: ""
    property string device: ""
    property string label: ""
    property ComboBox comboBox: null
    property Row buttonRow: null

    Row {
        id: labelRow
        spacing: 5
        Led {
            id: led
            color: ClientManagerLite.updateLED(device, name)
            anchors.verticalCenter: parent.verticalCenter
        }

        Kirigami.Label {
            text: label
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Item {
        id: propertyHolder
        Layout.fillHeight: true
        Layout.fillWidth: true
    }

    Rectangle {
        height: num.dp
        color: "grey"
        width: Screen.width
    }

    Connections {
        target: ClientManagerLite
        onNewINDISwitch: {
            if(deviceName == device) {
                if(name == propName) {
                    led.color = ClientManagerLite.updateLED(device, name)
                }
            }
        }
        onCreateINDIButton: {
            if(deviceName == device) {
                if(name == propName) {
                    if(buttonRow == null) {
                        var buttonRowComp = Qt.createComponent("KSButtonsSwitchRow.qml");
                        buttonRow = buttonRowComp.createObject(columnProp)
                        buttonRow.deviceName = device
                        buttonRow.propName = name
                        buttonRow.exclusive = exclusive
                    }
                    buttonRow.addButton(propText, switchName, checked, enabled)
                }
            }
        }
        onCreateINDIRadio: {
            if(deviceName == device) {
                if(name == propName) {
                    if(buttonRow == null) {
                        var buttonRowComp = Qt.createComponent("KSButtonsSwitchRow.qml");
                        buttonRow = buttonRowComp.createObject(columnProp)
                        buttonRow.deviceName = device
                        buttonRow.propName = name
                        buttonRow.exclusive = exclusive
                        buttonRow.checkBox = true
                    }
                    buttonRow.addCheckBox(propText, switchName, checked, enabled)
                }
            }
        }
        onCreateINDIMenu: {
            if(device == deviceName) {
                if(name == propName) {
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
            if(deviceName == device) {
                if(name == propName) {
                    var indiTextComp = Qt.createComponent("KSINDIText.qml");
                    var indiText = indiTextComp.createObject(columnProp)
                    indiText.addField(false, deviceName, propName, fieldName, propText, write)
                    indiText.propLabel = propLabel
                }
            }
        }
        onCreateINDINumber: {
            if(deviceName == device) {
                if(name == propName) {
                    var indiNumComp = Qt.createComponent("KSINDIText.qml");
                    var indiNum = indiNumComp.createObject(columnProp)
                    indiNum.addField(true, deviceName, propName, numberName, propText, write)
                    indiNum.propLabel = propLabel
                }
            }
        }
    }
}
