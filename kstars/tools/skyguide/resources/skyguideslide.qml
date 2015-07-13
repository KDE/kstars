import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

ColumnLayout {
    id: slide
    spacing: 10
    Layout.alignment: Qt.AlignHCenter

    ObjHeader {}

    Text {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        color: "#1703ca"
        font.pixelSize: 24
        style: Text.Raised
        text: loader.modelData.slideTitle
    }

    Image {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumHeight: parent.width * 0.5
        Layout.maximumWidth: parent.width * 0.9
        fillMode: Image.PreserveAspectFit
        source: loader.modelData.slideImgPath
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
