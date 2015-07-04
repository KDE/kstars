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

    ObjTextHeader {
        text: loader.modelData.title
    }

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: parent.width * 0.9
        Layout.preferredHeight: 200
        border.width: frameBorderWidth

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

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: parent.width * 0.9
        Layout.preferredHeight: 100
        border.width: frameBorderWidth

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
            Text { text: loader.modelData.summary.length; font.pixelSize: fontSizeText; }
        }
    }

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: parent.width * 0.9
        Layout.preferredHeight: 300
        border.width: frameBorderWidth

        Component {
            id: summaryDelegate
            Item {
                width: summary.width
                height: 25
                Text {
                    text: title
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: summary.currentIndex = index
                    onDoubleClicked: loadSlide(summary.currentIndex)
                }
            }
        }

        ListView {
            id: summary
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height - frameHMargin
            width: parent.width - frameVMargin
            focus: true
            model: ListModel {}
            highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
            delegate: summaryDelegate
            Keys.onReturnPressed: loadSlide(currentIndex)
            Component.onCompleted: {
                var s = loader.modelData.summary;
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
