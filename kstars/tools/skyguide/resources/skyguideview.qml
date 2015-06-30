import QtQuick 2.2

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
        source: "skyguidehome.qml"
    }
}
