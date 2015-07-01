import QtQuick 2.2

Rectangle {
    id: home

    function loadSlide(modelData) {
        loader.modelData = modelData;
        loader.source = "skyguideslide.qml";
    }

    Component {
        id: guidesDelegate
        Item {
            property var modelData: model.modelData
            width: home.width
            height: 25
            Column {
                Text { text: title }
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onClicked: guidesList.currentIndex = index
                onDoubleClicked: loadSlide(model.modelData)
            }
        }
    }

    Text {
        text: "Sky Guide"
        color: "#fd2121"
        font.pointSize: 21
        horizontalAlignment: Text.AlignHCenter
        anchors.fill: parent
    }

    ListView {
        id: guidesList
        focus: true
        anchors.bottomMargin: 16
        anchors.rightMargin: 26
        anchors.leftMargin: 25
        anchors.topMargin: 74
        boundsBehavior: Flickable.StopAtBounds
        anchors.fill: parent
        model: guidesModel
        highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
        delegate: guidesDelegate
        Keys.onReturnPressed: loadSlide(currentItem.modelData)
    }
}
