import QtQuick 1.0

Rectangle {
    id : base
    width: 333
    height: 575
    anchors.fill: parent
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
        objectName: "container"
        x: 14
        y: 73
        width: 305
        height: 379
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

            signal soListItemClicked( string type, int curIndex )
            clip: true
            visible: false

            delegate: Item {
                id: soListItem
                objectName: type
                x: 5
                height: 40
                Text {
                    id: dispText
                    objectName: dispName
                    text: dispName
                    color: "#ffffff"
                    anchors.verticalCenter: parent.verticalCenter
                    font.bold: true
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            soListView.currentIndex = index
                            soListView.soListItemClicked( soListItem.objectName , soListView.currentIndex)
                        }
                    }
                }
            }

            model: soListModel
        }
        Rectangle {
            id: detailsView
            color: "#00000000"
            radius: 5
            objectName: "detailsViewObj"
            anchors.fill: parent
            visible: false

            Text {
                id: soname
                objectName: "sonameObj"
                x: 5
                y: 5
                width: 273
                height: 44
                color: "#ffffff"
                text: qsTr("text")
                font.bold: true
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 16
            }

            Text {
                id: posText
                objectName: "posTextObj"
                x: 5
                y: 49
                width: 291
                height: 26
                color: "#ffffff"
                text: qsTr("text")
                horizontalAlignment: Text.AlignRight
                font.underline: true
                font.italic: true
                font.bold: true
                font.pixelSize: 10
            }

            Rectangle {
                x: 0
                y: 84
                width: 305
                height: 175
                color: "#00000000"
                radius: 10
                border.color: "#ffffff"
                Flickable {
                    id: flickable1
                    clip: true
                    flickableDirection: Flickable.VerticalFlick
                    //anchors.fill: parent
                    width: parent.width
                    height: parent.height
                    anchors.top: parent.top
                    anchors.topMargin: 3
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 4

                    contentWidth: parent.width
                    contentHeight: col.height

                    Column {
                        id: col
                        width: 300
                        Text {
                            id: descText
                            objectName: "descTextObj"
                            color: "#187988"
                            text: qsTr("text")
                            anchors.top: parent.top
                            anchors.topMargin: 3
                            anchors.left: parent.left
                            anchors.leftMargin: 4
                            clip: true
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            width: parent.width
                            //anchors.fill: parent
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Text {
                id: nextObjText
                objectName: "nextObj"
                x: 195
                y: 359
                width: 101
                height: 15
                color: "#ffffff"
                text: qsTr("Next sky-object")
                visible: true
                verticalAlignment: Text.AlignBottom
                horizontalAlignment: Text.AlignRight
                font.bold: true
                font.underline: true
                font.pixelSize: 11

                signal nextObjTextClicked()

                MouseArea {
                    id: mouse_area2
                    anchors.fill: parent
                    onClicked: nextObjText.nextObjTextClicked()
                }
            }

            Text {
                id: magText
                objectName: "magTextObj"
                x: 102
                y: 285
                width: 80
                height: 15
                color: "#ffffff"
                text: qsTr("text")
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 12
            }
        }
    }

    Text {
        id: soTypeText
        objectName: "soTypeTextObj"
        x: 189
        y: 46
        width: 130
        height: 20
        color: "#568656"
        text: qsTr("text")
        font.underline: true
        font.italic: true
        font.bold: true
        verticalAlignment: Text.AlignBottom
        horizontalAlignment: Text.AlignRight
        font.pixelSize: 12
        visible: false
    }
}
