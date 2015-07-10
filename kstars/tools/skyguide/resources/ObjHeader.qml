import QtQuick 2.2
import QtQuick.Layouts 1.1

Rectangle {
    Layout.alignment: Qt.AlignTop
    Layout.fillWidth: true
    Layout.preferredHeight: 50
    gradient: Gradient {
        GradientStop {
            position: 0.344
            color: "#000b5f"
        }
        GradientStop {
            position: 1
            color: "#0319ca"
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true

        Text {
            Layout.alignment: Qt.AlignHCenter
            color: "#ffffff"
            font.pixelSize: 24
            style: Text.Raised
            text: loader.modelData.title
        }

        Text {
            Layout.alignment: Qt.AlignBaseline | Qt.AlignLeft
            color: "#80b5f1"
            font.pixelSize: 12
            text: loader.modelData.slideTitle
        }
    }
}
