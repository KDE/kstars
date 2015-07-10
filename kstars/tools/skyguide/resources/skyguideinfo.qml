import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

ColumnLayout {
    id: info
    spacing: 10
    Layout.alignment: Qt.AlignHCenter

    function loadSlide(index) {
        loader.modelData.currentSlide = index;
        loader.source = "skyguideslide.qml";
    }

    ObjHeader {}

    ObjRectangle {
        TextArea {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height - frameHMargin
            width: parent.width - frameVMargin
            frameVisible: false
            font.pixelSize: fontSizeText
            readOnly: true
            text: loader.modelData.description
        }
    }

    ObjRectangle {
        Layout.maximumHeight: 100

        GridLayout {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height - frameHMargin
            width: parent.width - frameVMargin
            columns: 2

            Text { text: "Creation Date:"; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.creationDate; font.pixelSize: fontSizeText; }

            Text { text: "Language:"; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.language; font.pixelSize: fontSizeText; }

            Text { text: "Version:"; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.version; font.pixelSize: fontSizeText; }

            Text { text: "Slides:"; font.bold: true; font.pixelSize: fontSizeText; }
            Text { text: loader.modelData.contents.length; font.pixelSize: fontSizeText; }
        }
    }

    ObjRectangle {
        Component {
            id: contentsDelegate
            Item {
                width: contentsView.width
                height: 25
                Text {
                    text: title
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: contentsView.currentIndex = index
                    onDoubleClicked: loadSlide(contentsView.currentIndex)
                }
            }
        }

        ListView {
            id: contentsView
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height - frameHMargin
            width: parent.width - frameVMargin
            focus: true
            model: ListModel {}
            highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
            delegate: contentsDelegate
            Keys.onReturnPressed: loadSlide(currentIndex)
            Component.onCompleted: {
                var s = loader.modelData.contents;
                for (var key in s) {
                    model.append({"title": s[key]});
                }
            }
        }
    }

    Item {
        Layout.fillHeight: true;
    }
}
