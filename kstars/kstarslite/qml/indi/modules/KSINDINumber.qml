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

    function addText(propText, writable) {
        var textItem
        if(writable) {
            var textComp = Qt.createComponent("KSINDINumberField.qml");
            textItem = textComp.createObject(columnProp)
        } else {
            textItem = Qt.createQmlObject('import QtQuick 2.4
                                        import QtQuick.Layouts 1.2
                                    Text {
                                    }', this)
        }
        textItem.textField.text = propText
        //textItem.anchors.top = separator.bottom
    }
}
