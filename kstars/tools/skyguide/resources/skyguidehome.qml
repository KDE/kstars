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
        x: 73
        y: -62
        color: "#fd2121"
        anchors.centerIn: parent
        text: "Sky Guide"
        font.pointSize: 21
        anchors.verticalCenterOffset: -134
        anchors.horizontalCenterOffset: -3
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
