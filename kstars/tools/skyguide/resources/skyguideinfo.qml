import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

ColumnLayout {
    id: info
    spacing: 10
    Layout.alignment: Qt.AlignHCenter

    Text {
        Layout.fillWidth: true
        text: loader.modelData.title
        font.pixelSize: 24
        color: "#fd2121"
        horizontalAlignment: Text.AlignHCenter
    }

    TextArea {
        Layout.preferredWidth: parent.width * 0.9
        Layout.alignment: Qt.AlignHCenter
        text: loader.modelData.description
        readOnly: true
        font.pixelSize: 12
    }

    Item {
        Layout.fillHeight: true;
    }
}
