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
        font.pixelSize: fontSizeText
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

            Text { text: "Creation Date:"; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.creationDate; font.pixelSize: fontSizeText; }

            Text { text: "Language:"; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.language; font.pixelSize: fontSizeText; }

            Text { text: "Version:"; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.version; font.pixelSize: fontSizeText; }
        }
    }

    Item {
        Layout.fillHeight: true;
    }
}
