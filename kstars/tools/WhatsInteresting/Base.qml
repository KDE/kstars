import QtQuick 1.0

Rectangle {
    id : base
    width: 800
    height: 550
    gradient: Gradient {
        GradientStop {
            position: 0
            color: "#000000"
        }

        GradientStop {
            position: 1
            color: "#3555e4"
        }
    }

    Text {
        id: title
        x: 14
        y: 25
        width: 195
        height: 35
        color: "#dee8f1"
        text: qsTr("What's Interesting...")
        font.family: "Cantarell"
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 19
    }

    Rectangle {
        id: rectangle1
        x: 14
        y: 504
        width: 200
        height: 31
        color: "#00000000"

        Text {
            id: text1
            color: "#ffffff"
            text: qsTr("What's Interesting Settings")
            font.underline: true
            verticalAlignment: Text.AlignVCenter
            anchors.fill: parent
            font.pixelSize: 12

            MouseArea {
                id: mouse_area1
                x: 0
                y: 0
                anchors.fill: parent
            }
        }
    }

    Rectangle {
        objectName: "listContainer"
        x: 14
        y: 86
        width: 305
        height: 360
        color: "#00000000"
        border.color: "#ffffff"
        ListView {
            id: catListView
            objectName: "catListObj"
            anchors.fill: parent
            visible: true

            signal catListItemClicked(string category)

            delegate: Item {
                id: baseListItem
                objectName: modelData
                x: 5
                height: 40
                //property variant item: modelData
                Text {
                    text: modelData
                    color: "#ffffff"
                    anchors.verticalCenter: parent.verticalCenter
                    font.bold: true
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            catListView.currentIndex = index
                            switch (catListView.currentItem.objectName)
                            {
                            case "Planets" :
                            case "Satellites" :
                            case "Star" :
                            case "Galaxies" :
                            case "Constellations" :
                            case "Star Clusters" :
                            case "Nebulae" :
                                catListView.visible = false
                                soListView.visible = true
                                break;
                            }

                            catListView.catListItemClicked(catListView.currentItem.objectName)
                        }
                    }
                }
            }

            model: catListModel

//             ListModel {
//                 id : catListModel
//             }
        }
        ListView {
            id: soListView
            objectName: "soListObj"
            anchors.fill: parent

            signal soListItemClicked( string name, string category)
            visible: false

            delegate: Item {
                id: soListItem
                objectName: dispName
                x: 5
                height: 40
                Text {
                    text: dispName
                    color: "#ffffff"
                    anchors.verticalCenter: parent.verticalCenter
                    font.bold: true
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            soListView.currentIndex = index
                            soListView.catListItemClicked(soListView.currentItem.objectName)
                        }
                    }
                }
            }

            model: soListModel
        }
    }

    Rectangle {
        id: rectangle2
        x: 422
        y: 86
        width: 342
        height: 412
        color: "#1d4397"
    }
}
