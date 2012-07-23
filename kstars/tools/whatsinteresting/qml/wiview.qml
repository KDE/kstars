import QtQuick 1.0

Rectangle {
    id: container
    objectName: "containerObj"
    width: 370
    height: 575
    color: "#000000"
    anchors.fill: parent

    Text {
        id: title
        x: 9
        y: 34
        width: 192
        height: 46
        color: "#59ad0e"
        text: qsTr("What's Interesting...")
        verticalAlignment: Text.AlignVCenter
        font.family: "Liberation Sans"
        font.bold: false
        font.pixelSize: 19
    }

    Rectangle {
        id: base
        x: 0
        y: 91
        height: 414
        color: "#00000000"
        radius: 3
        border.width: 3
        border.color: "#2d2424"
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        opacity: 1

        Item {
            id: viewsRow
            objectName: "viewsRowObj"
            width: parent.width
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            signal categorySelected(int category)

            Flipable {
                id: categoryView
                width: parent.width
                height: parent.height

                property bool flipped: false

                front: Image {
                    id: frontCanvas
                    x: 0
                    y: 11
                    anchors.fill: parent
                    fillMode: Image.Tile
                    source: "stripes.png"

                    Rectangle {
                        id: planetRect
                        x: 15
                        y: 32
                        width: 159
                        height: 172
                        color: rectColor
                        radius: 7
                        border.width: 2
                        border.color: borderColor

                        property color rectColor: "#1a1313"
                        property color onHoverColor: "black"
                        property color borderColor: "#e2d57d"

                        MouseArea {
                            id: planetMouseArea
                            anchors.fill: parent

                            Image {
                                id: planetImage
                                x: 14
                                y: 14
                                width: 132
                                height: 125
                                smooth: true
                                sourceSize.height: parent.height-14
                                sourceSize.width: parent.width
                                fillMode: Image.PreserveAspectFit
                                source: "planets.jpg"
                            }

                            Text {
                                id: planetText
                                x: 0
                                y: 151
                                width: 159
                                height: 21
                                color: "#b75912"
                                text: qsTr("Planets")
                                anchors.right: parent.right
                                anchors.rightMargin: 0
                                anchors.left: parent.left
                                anchors.leftMargin: 0
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 0
                                font.bold: true
                                font.family: "Cantarell"
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 16
                            }

                            hoverEnabled: true
                            onEntered: {
                                planetRect.color = "black"
                                planetRect.border.color = "white"
                            }

                            onExited: {
                                planetRect.color = "#1a1313"
                                planetRect.border.color = planetRect.borderColor
                            }

                            onClicked: {
                                viewsRow.categorySelected(0)
                                container.state = "soListState"
                            }
                        }
                    }

                    Rectangle {
                        id: starRect
                        x: 196
                        y: 32
                        width: 159
                        height: 172
                        color: rectColor
                        radius: 7
                        border.width: 2
                        border.color: borderColor

                        property color rectColor: "#1a1313"
                        property color onHoverColor: "black"
                        property color borderColor: "#e2d57d"

                        MouseArea {
                            id: starMouseArea
                            anchors.fill: parent

                            Image {
                                id: starImage
                                x: 14
                                y: 14
                                width: 132
                                height: 125
                                smooth: true
                                sourceSize.height: parent.height-14
                                sourceSize.width: parent.width
                                fillMode: Image.PreserveAspectFit
                                source: "stars.jpg"
                            }

                            Text {
                                id: starText
                                x: 0
                                y: 151
                                width: 159
                                height: 21
                                color: "#b75912"
                                text: qsTr("Stars")
                                anchors.right: parent.right
                                anchors.rightMargin: 0
                                anchors.left: parent.left
                                anchors.leftMargin: 0
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 0
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                font.family: "Cantarell"
                                font.pixelSize: 16
                            }

                            hoverEnabled: true
                            onEntered: {
                                starRect.color = "black"
                                starRect.border.color = "white"
                            }

                            onExited: {
                                starRect.color = "#1a1313"
                                starRect.border.color = starRect.borderColor
                            }

                            onClicked: {
                                viewsRow.categorySelected(1)
                                container.state = "soListState"
                            }
                        }
                    }

                    Rectangle {
                        id: conRect
                        x: 15
                        y: 219
                        width: 159
                        height: 172
                        color: rectColor
                        radius: 7
                        border.width: 2
                        border.color: borderColor

                        property color rectColor: "#1a1313"
                        property color onHoverColor: "black"
                        property color borderColor: "#e2d57d"

                        MouseArea {
                            id: conMouseArea
                            anchors.fill: parent

                            Text {
                                id: conText
                                x: 0
                                y: 151
                                width: 159
                                height: 21
                                color: "#b75912"
                                text: qsTr("Constellations")
                                font.family: "Cantarell"
                                anchors.right: parent.right
                                anchors.rightMargin: 0
                                anchors.left: parent.left
                                anchors.leftMargin: 0
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 0
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 16
                            }

                            Image {
                                id: conImage
                                x: 14
                                y: 14
                                width: 132
                                height: 125
                                sourceSize.height: parent.height-14
                                sourceSize.width: parent.width
                                source: "constellation.jpg"
                            }

                            hoverEnabled: true
                            onEntered: {
                                conRect.color = "black"
                                conRect.border.color = "white"
                            }

                            onExited: {
                                conRect.color = "#1a1313"
                                conRect.border.color = conRect.borderColor
                            }

                            onClicked: {
                                viewsRow.categorySelected(2)
                                container.state = "soListState"
                            }
                        }
                    }

                    Rectangle {
                        id: dsoRect
                        x: 196
                        y: 219
                        width: 159
                        height: 172
                        color: rectColor
                        radius: 7
                        border.width: 2
                        border.color: borderColor

                        property color rectColor: "#1a1313"
                        property color onHoverColor: "black"
                        property color borderColor: "#e2d57d"

                        MouseArea {
                            id: dsoMouseArea
                            anchors.fill: parent

                            Image {
                                id: dsoImage
                                x: 14
                                y: 14
                                width: 132
                                height: 125
                                smooth: true
                                fillMode: Image.PreserveAspectFit
                                sourceSize.height: parent.height-14
                                sourceSize.width: parent.width
                                source: "dso.jpg"
                            }

                            Text {
                                id: text1
                                x: 0
                                y: 151
                                width: 159
                                height: 21
                                color: "#b75912"
                                text: qsTr("Deep-sky Objects")
                                font.family: "Cantarell"
                                anchors.right: parent.right
                                anchors.rightMargin: 0
                                anchors.left: parent.left
                                anchors.leftMargin: 0
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 0
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 16
                            }

                            hoverEnabled: true
                            onEntered: {
                                dsoRect.color = "black"
                                dsoRect.border.color = "white"
                            }

                            onExited: {
                                dsoRect.color = "#1a1313"
                                dsoRect.border.color = dsoRect.borderColor
                            }

                            onClicked: {
                                categoryView.state = "back"
                                container.state = "dsoCategoryView"
                            }
                        }
                    }
                }

                back: Image {
                    id: backCanvas
                    x: 0
                    y: 11
                    anchors.fill: parent
                    fillMode: Image.Tile
                    source: "stripes.png"

                    Rectangle {
                        id: galaxyRect
                        x: 15
                        y: 32
                        width: 159
                        height: 172
                        color: rectColor
                        border.color: borderColor
                        radius: 7
                        border.width: 2

                        property color rectColor: "#1a1313"
                        property color onHoverColor: "black"
                        property color borderColor: "#e2d57d"

                        MouseArea {
                            id: galaxyMouseArea
                            anchors.fill: parent

                            Image {
                                id: galaxyImage
                                x: 14
                                y: 14
                                width: 132
                                height: 125
                                smooth: true
                                sourceSize.height: parent.height-14
                                sourceSize.width: parent.width
                                fillMode: Image.PreserveAspectFit
                                source: "galaxy.jpg"
                            }

                            Text {
                                id: galaxyText
                                x: 0
                                y: 151
                                width: 159
                                height: 21
                                color: "#b75912"
                                text: qsTr("Galaxies")
                                anchors.right: parent.right
                                anchors.rightMargin: 0
                                anchors.left: parent.left
                                anchors.leftMargin: 0
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 0
                                font.bold: true
                                font.family: "Cantarell"
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 16
                            }

                            hoverEnabled: true
                            onEntered: {
                                galaxyRect.color = "black"
                                galaxyRect.border.color = "white"
                            }

                            onExited: {
                                galaxyRect.color = "#1a1313"
                                galaxyRect.border.color = galaxyRect.borderColor
                            }

                            onClicked: {
                                viewsRow.categorySelected(3)
                                container.state = "soListState"
                            }
                        }
                    }

                    Rectangle {
                        id: clustRect
                        x: 15
                        y: 219
                        width: 159
                        height: 172
                        color: rectColor
                        radius: 7
                        border.width: 2
                        border.color: borderColor
                        anchors.horizontalCenter: parent.horizontalCenter

                        property color rectColor: "#1a1313"
                        property color onHoverColor: "black"
                        property color borderColor: "#e2d57d"

                        MouseArea {
                            id: clustMouseArea
                            anchors.fill: parent

                            Text {
                                id: clustText
                                x: 0
                                y: 151
                                width: 159
                                height: 21
                                color: "#b75912"
                                text: qsTr("Clusters")
                                font.family: "Cantarell"
                                anchors.right: parent.right
                                anchors.rightMargin: 0
                                anchors.left: parent.left
                                anchors.leftMargin: 0
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 0
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 16
                            }

                            Image {
                                id: clustImage
                                x: 14
                                y: 13
                                width: 132
                                height: 125
                                sourceSize.height: parent.height-14
                                sourceSize.width: parent.width
                                source: "cluster.jpg"
                            }

                            hoverEnabled: true
                            onEntered: {
                                clustRect.color = "black"
                                clustRect.border.color = "white"
                            }

                            onExited: {
                                clustRect.color = "#1a1313"
                                clustRect.border.color = clustRect.borderColor
                            }

                            onClicked: {
                                viewsRow.categorySelected(4)
                                container.state = "soListState"
                            }
                        }
                    }

                    Rectangle {
                        id: nebRect
                        x: 196
                        y: 32
                        width: 159
                        height: 172
                        color: rectColor
                        radius: 7
                        border.width: 2
                        border.color: borderColor

                        property color rectColor: "#1a1313"
                        property color onHoverColor: "black"
                        property color borderColor: "#e2d57d"

                        MouseArea {
                            id: nebMouseArea
                            anchors.fill: parent

                            Image {
                                id: nebImage
                                x: 14
                                y: 14
                                width: 132
                                height: 125
                                smooth: true
                                sourceSize.height: parent.height-14
                                sourceSize.width: parent.width
                                fillMode: Image.PreserveAspectFit
                                source: "nebula.jpg"
                            }

                            Text {
                                id: nebText
                                x: 0
                                y: 151
                                width: 159
                                height: 21
                                color: "#b75912"
                                text: qsTr("Nebulae")
                                anchors.right: parent.right
                                anchors.rightMargin: 0
                                anchors.left: parent.left
                                anchors.leftMargin: 0
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 0
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                font.family: "Cantarell"
                                font.pixelSize: 16
                            }

                            hoverEnabled: true
                            onEntered: {
                                nebRect.color = "black"
                                nebRect.border.color = "white"
                            }

                            onExited: {
                                nebRect.color = "#1a1313"
                                nebRect.border.color = nebRect.borderColor
                            }

                            onClicked: {
                                viewsRow.categorySelected(5)
                                container.state = "soListState"
                            }
                        }
                    }
                }

                states: [
                    State {
                        name: "back"
                        PropertyChanges {
                            target: canvasRotation
                            angle: 180
                        }
                    }
                ]

                transitions: [
                    Transition {
                        NumberAnimation { target: canvasRotation; property: "angle"; duration: 400 }
                    }
                ]

                transform: Rotation {
                    id: canvasRotation
                    origin.x: container.width / 2;
                    axis.y: 1; axis.z: 0
                }
            } //end of categoryView

            Flipable {
                id: skyObjView
                width: parent.width
                height: parent.height

                anchors.left: categoryView.right

                property bool flipped: false

                front: Rectangle {
                    id: soListContainer
                    height: parent.height
                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "#000000"
                        }

                        GradientStop {
                            position: 1
                            color: "#181b29"
                        }
                    }
                    width: parent.width

                    ListView {
                        id: soListView
                        objectName: "soListObj"
                        anchors.fill: parent

                        signal soListItemClicked( int type, string typeName, int curIndex )
                        clip: true

                        delegate: Item {
                            id: soListItem
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
                                        soListView.soListItemClicked(type, typeName, soListView.currentIndex)
                                        skyObjView.flipped = true
                                    }
                                }
                            }
                        }

                        model: soListModel
                    }
                }

                back: Rectangle {
                    id: detailsView
                    objectName: "detailsViewObj"
                    height: parent.height
                    width: parent.width
                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "#000000"
                        }

                        GradientStop {
                            position: 1
                            color: "#181b29"
                        }
                    }

                    anchors.left: categoryView.right

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
                        width: parent.width
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
                                width: parent.width
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
                        x: parent.width - 101
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
                            id: nextObjMouseArea
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
                        anchors.horizontalCenter: parent.horizontalCenter
                        font.pixelSize: 12
                    }
                }

                states: [
                    State {
                        name: "back"
                        PropertyChanges {
                            target: listToDetailsRotation
                            angle: 180
                        }
                        when: skyObjView.flipped
                    }
                ]

                transitions: [
                    Transition {
                        NumberAnimation { target: listToDetailsRotation; property: "angle"; duration: 400 }
                    }
                ]

                transform: Rotation {
                    id: listToDetailsRotation
                    origin.x: container.width / 2;
                    axis.y: 1; axis.z: 0
                }
            } //end of skyObjView
        } // end of viewsRow
    } //end of base

    Button {
        id: settingsButton
        x: 22
        y: backButton.y
        width: 150
        height: 40
        text: "User Settings"
    }

    Button {
        id: backButton
        x: container.width + 10
        y: base.y + base.height + 10
        text: "Go back"
        onClicked: {
            if (container.state == "dsoCategoryView")
            {
                container.state = "base"
                categoryView.state = "front"
            }
        }
    }


    states: [
        State {
            name: "soListState"
            PropertyChanges {
                target: viewsRow
                x: -(parent.width)
            }
            PropertyChanges {
                target: backButton
                x: dsoRect.x + 5
            }
        },
        State {
            name: "dsoCategoryView"
            PropertyChanges {
                target: backButton
                x: dsoRect.x + 5
            }
        },
        State {
            name: "base"
            PropertyChanges {
                target: backButton
                x: container.width + 10
            }
        }
    ]
    transitions: [
        Transition {
            from: "*"; to: "soListState"
            NumberAnimation { target: viewsRow; property: "x"; duration: 900; easing.type: Easing.InOutQuad }
            NumberAnimation { target: backButton; property: "x"; duration: 400; easing.type: Easing.InOutQuad }
        },
        Transition {
            from: "*"; to: "dsoCategoryView";
            NumberAnimation { target: backButton; property: "x"; duration: 300; easing.type: Easing.InOutQuad }
        },
        Transition {
            from: "dsoCategoryView"; to: "base"
            NumberAnimation { target: backButton; property: "x"; duration: 300; easing.type: Easing.InOutQuad }
        }
    ]
}
