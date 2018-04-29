import QtQuick 2.5

Text {
    property string title
    property color activeColor:"#e4800d"
    property color disabledColor:"#6b6660"

    id: categoryTitle
    color: activeColor
    text: title
    anchors.centerIn: parent
    font {
        family: "Cantarell"
        pixelSize: 16
        bold: false
    }
    renderType: Text.NativeRendering

    states: [
        State {
            name: "selected"

            PropertyChanges {
                target: categoryTitle
                font {
                    pixelSize: 21
                    bold: true
                }
            }
        },
        State {
            name: "selectedNoBold"
            PropertyChanges {
                target: categoryTitle
                font {
                    pixelSize: 21
                    bold: false
                }
            }
        },
        State {
            name: ""

            PropertyChanges {
                target: categoryTitle
                font {
                    pixelSize: 16
                    bold: false
                }
            }
        }
    ]

    Behavior on font.pixelSize {
        NumberAnimation {
            duration: 150
        }
    }
}
