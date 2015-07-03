import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

ColumnLayout {
    id: view
    property int fontSizeText: 12

    Action {
        id: homeAction
        onTriggered: loader.source = "skyguidehome.qml"
    }

    ToolBar {
        Layout.alignment: Qt.AlignTop
        Layout.preferredWidth: 360
        Layout.fillWidth: true
        RowLayout {
            anchors.fill: parent
            ToolButton {
                width: 25
                height: 20
                iconSource: "home.png"
                action: homeAction
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
