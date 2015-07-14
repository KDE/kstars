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
        Text {
            Layout.alignment: Qt.AlignLeft
            Layout.fillWidth: true
            color: "#ffffff"
            font.pixelSize: 24
            style: Text.Raised
            text: loader.modelData.title
        }

        RowLayout {
            Text {
                id: slidepath
                color: "#80b5f1"
                font.pixelSize: 12
                text: "SkyGuide"
            }

            Text {
                id: patharrow
                color: "#6680b5f1"
                font.pixelSize: 12
                text: ">>"
            }

            Text {
                color: slidepath.color
                font: slidepath.font
                text: loader.modelData.title
            }

            Text {
                color: patharrow.color
                font: patharrow.font
                text: patharrow.text
            }

            Text {
                color: slidepath.color
                font: slidepath.font
                text: loader.modelData.slideTitle
            }
        }
    }
}
