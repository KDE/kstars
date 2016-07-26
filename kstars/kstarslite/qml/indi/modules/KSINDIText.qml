import QtQuick 2.4
import QtQuick.Layouts 1.2
import "../../constants" 1.0
import org.kde.kirigami 1.0 as Kirigami

Column {
    Layout.fillHeight: true
    spacing: 5 * num.dp
    width: parent.width
    property string propLabel: ""

    Kirigami.Label {
        text: propLabel
    }

    Rectangle {
        id:separator
        color: "grey"
        height: 1 * num.dp
        width: parent.width
    }

    function addField(isNumber, deviceName, propName, fieldName, propText, writable) {
        var textItem
        if(writable) {
                var textComp = Qt.createComponent("KSINDITextField.qml");
                textItem = textComp.createObject(columnProp)
                textItem.deviceName = deviceName
                textItem.propName = propName
                textItem.fieldName = fieldName
                textItem.textField.text = propText
                textItem.isNumber = isNumber
        } else {
                textItem = Qt.createQmlObject('import QtQuick 2.4
                                        import QtQuick.Layouts 1.2
                                    Text {

                                    }', this)
                textItem.text = propText
        }
        //textItem.anchors.top = separator.bottom
    }
}
