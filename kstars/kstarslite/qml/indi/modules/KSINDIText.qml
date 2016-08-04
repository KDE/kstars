import QtQuick 2.4
import QtQuick.Layouts 1.2
import "../../constants" 1.0
import org.kde.kirigami 1.0 as Kirigami

Column {
    id: columnTextProp
    Layout.fillHeight: true
    Layout.fillWidth: true
    spacing: 5 * num.dp
    property string propLabel: ""

    Kirigami.Label {
        text: propLabel
        color: num.sysPalette.text
    }

    Rectangle {
        id: separator
        height: num.dp
        color: num.sysPalette.light
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
                textItem = Qt.createQmlObject('import QtQuick 2.4
                                        import QtQuick.Layouts 1.2
                                        import "../../constants" 1.0
                                    Text {
                                        color: num.sysPalette.text
                                    }', this)
                textItem.text = propText
        }
        //textItem.anchors.top = separator.bottom
    }
}
