import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

Rectangle {
    id: view
    x: 0
    y: 0    
    width: 360
    height: 360

    Loader {
        id: loader
        focus: true
        anchors.fill: parent
        property var modelData: null
        anchors.topMargin: 46
        source: "skyguidehome.qml"
    }

    Action {
        id: homeAction
        onTriggered: loader.source = "skyguidehome.qml"
    }

    ToolBar {
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
}
