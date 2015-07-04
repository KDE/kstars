import QtQuick 2.2

Rectangle {
    id: home

    function loadGuide(modelData) {
        loader.modelData = modelData;
        loader.source = "skyguideinfo.qml";
    }

    Component {
        id: guidesDelegate
        Item {
            property var modelData: model.modelData
            width: guidesList.width
            height: 25
            Text {
                text: title
            }
            MouseArea {
                anchors.fill: parent
                onClicked: guidesList.currentIndex = index
                onDoubleClicked: loadGuide(model.modelData)
            }
        }
    }

    ObjTextHeader {
        text: "Sky Guide"
    }

    ListView {
        id: guidesList
        focus: true
        anchors.bottomMargin: 16
        anchors.rightMargin: 26
        anchors.leftMargin: 25
        anchors.topMargin: 74
        anchors.fill: parent
        model: guidesModel
        highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
        delegate: guidesDelegate
        Keys.onReturnPressed: loadGuide(currentItem.modelData)
    }
}
