import QtQuick 2.0
import QtQuick.Layouts 1.1
import "../../constants" 1.0

ColumnLayout {
    property var iconSrc: ""
    property string title: ""
    anchors {
        left: parent.left
        right: parent.right
    }
    DrawerElem {
        iconSrc: num.iconpath + "/color-management.png"
        title: "Color Scheme"

        MouseArea {
            anchors.fill: parent
            onPressed: {
                if (!colorSchemes.Layout.minimumHeight) colorSchemes.Layout.minimumHeight = colorsFlow.childrenRect.height
                else colorSchemes.Layout.minimumHeight = 0
            }

            Image {
                source: num.iconpath + "/arrow-right.png"
                anchors.right: parent.right
            }
        }

    }

    Rectangle {
        id:colorSchemes
        Layout.minimumHeight: 0
        Layout.fillWidth: true

        Behavior on Layout.minimumHeight {
            NumberAnimation {
                duration: 400
                easing.type: Easing.InOutQuad
            }
        }

        transitions: Transition {
            NumberAnimation {
                properties: "Layout.minimumHeight"
                easing.type: Easing.InOutQuad
            }
        }

        Flow {
            id: colorsFlow
            height: parent.height
            anchors {
                left: parent.left
                leftMargin: num.dp * 55
                top: parent.top
                topMargin: num.dp * 10
                right: parent.right
                rightMargin: num.dp * 20
            }

            clip: true
            property double rectSize: num.dp * 80
            spacing: num.dp * 20
            Rectangle {
                width: parent.rectSize
                height: parent.rectSize
                color: "#000022"
                border {
                    width: 1
                    color: "black"
                }
            }

            Rectangle {
                width: parent.rectSize
                height: parent.rectSize
                color: "#ffffff"
                border {
                    width: 1
                    color: "black"
                }
            }

            Rectangle {
                width: parent.rectSize
                height: parent.rectSize
                color: "#880000"
                border {
                    width: 1
                    color: "black"
                }
            }

            Rectangle {
                width: parent.rectSize
                height: parent.rectSize
                color: "black"
                border {
                    width: 1
                    color: "black"
                }
            }
        }
    }
}
