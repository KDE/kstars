import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

ColumnLayout {
    id: slide
    spacing: 10
    Layout.alignment: Qt.AlignHCenter

    ObjTextHeader {
        text: loader.modelData.slideTitle
    }

    ObjRectangle {
        TextArea {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height - frameHMargin
            width: parent.width - frameVMargin
            frameVisible: false
            font.pixelSize: fontSizeText
            readOnly: true
            text: loader.modelData.slideText
        }
    }
}
