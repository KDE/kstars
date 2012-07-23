import QtQuick 1.0

Rectangle {
    id: container
    width: 115
    height: 40
    radius: 12

    property string text
    signal clicked

    gradient: Gradient {
        GradientStop {
            position: 0
            color: "#484a55"
        }

        GradientStop {
            position: 1
            color: "#000000"
        }
    }

    MouseArea {
        id: buttonMouseArea
        anchors.fill: parent

        Text {
            id: buttonText
            x: 46
            y: 13
            color: "#d9bb11"
            text: container.text
            style: Text.Raised
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            font.family: "Cantarell"
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 16
        }

        onClicked: container.clicked()
    }
}
