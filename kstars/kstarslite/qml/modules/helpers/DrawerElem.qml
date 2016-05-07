import QtQuick 2.0
import QtQuick.Layouts 1.1
import "../../constants" 1.0

RowLayout {
    property var iconSrc: ""
    property string title: ""
    spacing: 25 * num.dp
    anchors {
        left: parent.left
        right: parent.right
    }

    Image {
        source: iconSrc
        fillMode: Image.PreserveAspectFit
    }

    Text {
        text: title
        font.family: "Oxygen Normal"
        font.pixelSize: num.dp * 28
        color: "#31363b"
        anchors.verticalCenter: parent.verticalCenter
    }
}
