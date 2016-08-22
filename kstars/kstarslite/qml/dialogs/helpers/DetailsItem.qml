import QtQuick 2.7
import QtQuick.Controls 2.0

Column {
    property string label
    property string value
    spacing: 5
    visible: value.length

    width: parent.width

    Label {
        text: label
        font.pointSize: 13
    }

    Rectangle {
        width: parent.width
        height: 1
        color: "grey"
    }

    Label {
        font.pointSize: 11
        text: value
    }
}
