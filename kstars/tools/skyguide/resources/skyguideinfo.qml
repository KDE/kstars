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
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: parent.width * 0.9
        text: loader.modelData.description
        readOnly: true
        font.pixelSize: 12
    }

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: parent.width * 0.9
        Layout.preferredHeight: 100
        color: "#ffffff"
        border.width: 1

        GridLayout {
            Layout.alignment: Qt.AlignHCenter
            columns: 2

            Text { text: "Creation Date:"; font.bold: true; }
            Text { text: loader.modelData.creationDate; }

            Text { text: "Language:"; font.bold: true; }
            Text { text: loader.modelData.language; }

            Text { text: "Version:"; font.bold: true; }
            Text { text: loader.modelData.version; }
        }
    }

    Item {
        Layout.fillHeight: true;
    }
}
