import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

ColumnLayout {
    id: view
    spacing: 0
    property int fontSizeText: 12
    property int frameBorderWidth: 1
    property int frameHMargin: 10
    property int frameVMargin: 10

    function loadHome() {
        loader.modelData.currentSlide = -1
        loader.source = "skyguidehome.qml"
    }

    function loadGuide(modelData) {
        loader.modelData = modelData;
        loader.source = "skyguideinfo.qml";
    }

    function loadSlide(index) {
        loader.modelData.currentSlide = index;
        loader.source = "skyguideslide.qml";
    }

    ToolBar {
        Layout.alignment: Qt.AlignTop
        Layout.preferredWidth: 360
        Layout.fillWidth: true
        RowLayout {
            anchors.fill: parent
            ToolButton {
                width: 20
                height: 20
                iconSource: "icons/home.png"
                onClicked: loadHome()
            }
            Item { Layout.fillWidth: true }
        }
    }

    Loader {
        id: loader
        Layout.alignment: Qt.AlignCenter
        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.minimumHeight: 360
        focus: true
        property var modelData: null
        source: "skyguidehome.qml"
    }

    Item {
        Layout.minimumHeight: 5
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    Rectangle {
        id: menu
        Layout.alignment: Qt.AlignBottom
        Layout.fillWidth: true
        Layout.preferredHeight: 46
        color: "#bcbcb5"
    }
}
