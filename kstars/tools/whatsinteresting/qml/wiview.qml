/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

import QtQuick 1.0

Rectangle {
    id: container
    objectName: "containerObj"
    width: 376
    height: 575
    color: "#020518"
    anchors.fill: parent

    Text {
        id: title
        x: 9
        y: 34
        width: 209
        height: 46
        color: "#59ad0e"
        text: qsTr("What's Interesting...")
        verticalAlignment: Text.AlignVCenter
        font.family: "Cantarell"
        font.bold: false
        font.pixelSize: 22
    }

    Rectangle {
        id: base
        y: 89
        width: parent.width
        height: 385
        color: "transparent"
        radius: 12
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        Item {
            id: viewsRow
            objectName: "viewsRowObj"
            width: parent.width
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            signal categorySelected(int category)

            Rectangle {
                id: categoryView
                x: 0
                y: 31
                width: parent.width
                height: 351
                color: "transparent"

                Rectangle {
                    id: background

                    color: "#00060b"
                    radius: 12
                    anchors.top: parent.top
                    anchors.topMargin: 15
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 13
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    opacity: 0.500
                    border.width: 4
                    border.color: "black"
                }

                Rectangle {
                    id: planetRect
                    x: 128
                    y: 35
                    width: planetText.width
                    height: planetText.height
                    color: "#00000000"
                    anchors.verticalCenterOffset: -130
                    anchors.horizontalCenterOffset: -30
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text {
                        id: planetText
                        x: 0
                        y: 0
                        color: "#e4800d"
                        text: qsTr("Planets")
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        font.family: "Cantarell"
                        font.pixelSize: 16

                        MouseArea {
                            id: planetMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: container.state = "planetAreaEntered"
                            onClicked: {
                                viewsRow.categorySelected(0)
                                container.state = "soTypeSelected"
                            }
                        }
                    }
                }

                Rectangle {
                    id: starRect
                    x: 253
                    y: 80
                    width: starText.width
                    height: starText.height
                    color: "#00000000"
                    anchors.verticalCenterOffset: -85
                    anchors.horizontalCenterOffset: 87
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: starText
                        x: 0
                        y: 0
                        color: "#e4800d"
                        text: qsTr("Stars")
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        font.family: "Cantarell"
                        font.pixelSize: 16

                        MouseArea {
                            id: starMouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            onEntered: container.state = "starAreaEntered"
                            onClicked: {
                                viewsRow.categorySelected(1)
                                container.state = "soTypeSelected"
                            }
                        }
                    }
                }

                Rectangle {
                    id: conRect
                    x: 71
                    y: 113
                    width: conText.width
                    height: conText.height
                    color: "#00000000"
                    anchors.verticalCenterOffset: -52
                    anchors.horizontalCenterOffset: -63
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: conText
                        color: "#e4800d"
                        text: qsTr("Constellations")
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                        font.family: "Cantarell"
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: 16

                        MouseArea {
                            id: conMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: container.state = "conAreaEntered"
                            onClicked: {
                                viewsRow.categorySelected(2)
                                container.state = "soTypeSelected"
                            }
                        }
                    }
                }

                Rectangle {
                    id: dsoContainer
                    y: 172
                    height: 166
                    color: "#00000000"
                    anchors.right: parent.right
                    anchors.rightMargin: 35
                    anchors.left: parent.left
                    anchors.leftMargin: 35

                    Rectangle {
                        id: dsoRect
                        x: 79
                        y: 18
                        width: dsoText.width
                        height: dsoText.height
                        color: "#00000000"
                        anchors.verticalCenterOffset: -54
                        anchors.horizontalCenterOffset: 0
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            id: dsoText
                            x: 0
                            y: 0
                            color: "#e4800d"
                            text: qsTr("Deep-sky Objects")
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 16
                            MouseArea {
                                id: dsoMouseArea
                                hoverEnabled: true
                                anchors.fill: parent
                                onEntered: container.state = "dsoAreaEntered"
                                onClicked: container.state = "dsoAreaClicked"
                            }
                            font.family: "Cantarell"
                        }
                    }

                    Rectangle {
                        id: galRect
                        x: 35
                        y: 68
                        width: galText.width
                        height: galText.height
                        color: "#00000000"
                        anchors.verticalCenterOffset: -4
                        anchors.horizontalCenterOffset: -77
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        opacity: 0.350
                        Text {
                            id: galText
                            x: 0
                            y: 0
                            color: "#6b6660"
                            text: qsTr("Galaxies")
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 16
                            anchors.topMargin: 0
                            MouseArea {
                                id: galMouseArea
                                enabled: false
                                hoverEnabled: false
                                anchors.fill: parent
                                onEntered: container.state = "galAreaEntered"
                                onClicked: {
                                    viewsRow.categorySelected(3)
                                    container.state = "dsoTypeSelected"
                                }
                            }
                            anchors.rightMargin: 0
                            anchors.bottomMargin: 0
                            font.family: "Cantarell"
                            anchors.leftMargin: 0
                        }
                    }

                    Rectangle {
                        id: nebRect
                        x: 96
                        y: 124
                        width: nebText.width
                        height: nebText.height
                        color: "#00000000"
                        anchors.verticalCenterOffset: 52
                        anchors.horizontalCenterOffset: -17
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        opacity: 0.340
                        Text {
                            id: nebText
                            x: 0
                            y: 0
                            color: "#6b6660"
                            text: qsTr("Nebulae")
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 16
                            MouseArea {
                                id: nebMouseArea
                                enabled: false
                                hoverEnabled: false
                                anchors.fill: parent
                                onEntered: container.state = "nebAreaEntered"
                                onClicked: {
                                    viewsRow.categorySelected(5)
                                    container.state = "dsoTypeSelected"
                                }
                            }
                            font.family: "Cantarell"
                        }
                    }

                    Rectangle {
                        id: clustRect
                        x: 181
                        y: 80
                        width: clustText.width
                        height: clustText.height
                        color: "#00000000"
                        anchors.verticalCenterOffset: 8
                        anchors.horizontalCenterOffset: 69
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        opacity: 0.350
                        Text {
                            id: clustText
                            x: 0
                            y: 0
                            color: "#6b6660"
                            text: qsTr("Clusters")
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 16
                            MouseArea {
                                id: clustMouseArea
                                enabled: false
                                hoverEnabled: false
                                anchors.fill: parent
                                onEntered: container.state = "clustAreaEntered"
                                onClicked: {
                                    viewsRow.categorySelected(4)
                                    container.state = "dsoTypeSelected"
                                }
                            }
                            font.family: "Cantarell"
                        }
                    }
                }
            }//end of categoryView

            Flipable {
                id: skyObjView
                width: parent.width
                height: parent.height
                anchors.leftMargin: categoryView.width
//                anchors.leftMargin: 370

                anchors.left: categoryView.right

                property bool flipped: false

                front: Rectangle {
                    id: soListContainer
                    color: "transparent"
                    anchors.fill: parent

                    Rectangle {
                        id: soListViewBackground
                        anchors.fill: soListViewContainer
                        color: "#00060b"
                        opacity: 0.5
                        radius: 12
                    }

                    Rectangle {

                        id: soListViewContainer
                        x: parent.x + 15
                        y: 31
                        width: parent.width - 30
                        height: 351
                        color: "transparent"
                        radius: 12
                        border.width: 4
                        border.color: "#000000"

                        ListView {
                            id: soListView
                            objectName: "soListObj"
                            anchors.fill: parent

                            signal soListItemClicked(int type, string typeName, int curIndex)
                            clip: true

                            ScrollBar {
                                flickable: soListView
                            }

                            delegate: Item {
                                id: soListItem
                                x: 5
                                height: 40

                                Text {
                                    id: dispText
                                    objectName: dispName
                                    text: dispName
                                    color: "white"
                                    anchors.verticalCenter: parent.verticalCenter
                                    font.bold: true
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onEntered: dispText.color = "yellow"
                                        onExited: dispText.color = "white"
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
                }

                back: Rectangle {
                    id: detailsViewContainer
                    width: parent.width
                    height: parent.height
                    color: "transparent"

                    Rectangle {
                        id: detailsViewBackground
                        anchors.fill: detailsView
                        color: "#00060b"
                        radius: 12
                        opacity: 0.500
                    }

                    Rectangle {
                        id: detailsView
                        objectName: "detailsViewObj"
                        x: parent.x + 15
                        height: 415
                        width: parent.width - 30
                        color: "transparent"
                        radius: 12
                        border.width: 4
                        border.color: "#000000"

                        Text {
                            id: soname
                            objectName: "sonameObj"
                            y: 8
                            width: 273
                            height: 22
                            color: "#ffffff"
                            text: qsTr("text")
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            font.bold: true
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 16
                        }

                        Text {
                            id: posText
                            x: 10
                            objectName: "posTextObj"
                            y: 45
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            width: 320
                            height: 16
                            color: "#f7f7ac"
                            text: qsTr("text")
                            font.family: "Cantarell"
                            horizontalAlignment: Text.AlignRight
                            font.underline: false
                            font.italic: false
                            font.bold: true
                            font.pixelSize: 11
                        }

                        Rectangle {
                            id: descTextBox
                            y: 197
                            height: 175
                            color: "#010a14"
                            radius: 10
                            border.width: 0
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.left: parent.left
                            anchors.leftMargin: 4
                            border.color: "#585454"
                            Flickable {
                                id: flickableDescText
                                clip: true
                                flickableDirection: Flickable.VerticalFlick
                                width: parent.width
                                height: parent.height
                                anchors.top: parent.top
                                anchors.topMargin: 3
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 4

                                contentWidth: parent.width
                                contentHeight: col.height + 4

                                Item {
                                    id: descTextItem
                                    anchors.top :parent.top
                                    anchors.topMargin: 3
                                    anchors.left: parent.left
                                    anchors.leftMargin: 6
                                    anchors.right: parent.right
                                    Column {
                                        id: col
                                        width: parent.width
                                        Text {
                                            id: descText
                                            objectName: "descTextObj"
                                            color: "#187988"
                                            text: qsTr("text")
                                            font.family: "Cantarell"
                                            clip: true
                                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                            width: parent.width
                                            font.pixelSize: 13
                                        }
                                        Text {
                                            id: descSrcText
                                            objectName: "descSrcTextObj"
                                            color: "#18885f"
                                            text: qsTr("Source: ")
                                            font.family: "Cantarell"
                                            font.italic: true
                                            horizontalAlignment: Text.AlignRight
                                            clip: true
                                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                            width: parent.width
                                            font.pixelSize: 13
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            id: nextObjRect
                            objectName: "nextObj"
                            x: 271
                            y: 377
                            width: nextObjText.width + nextObjIcon.width + 5
                            height: 28
                            color: "#00000000"
                            radius: 5
                            anchors.right: parent.right
                            anchors.rightMargin: 10

                            signal nextObjClicked()

                            Rectangle {
                                id: nextObjForeground
                                radius: 5
                                anchors.fill: nextObjRect
                                opacity: 0
                            }

                            MouseArea {
                                id: nextObjMouseArea
                                x: 13
                                y: 62
                                anchors.fill: nextObjRect
                                hoverEnabled: true
                                onEntered: {
                                    nextObjForeground.opacity = 0.1
                                    nextObjText.color = "yellow"
                                }
                                onExited: {
                                    nextObjForeground.opacity = 0.0
                                    nextObjText.color = "white"
                                }
                                onClicked: nextObjRect.nextObjClicked()
                            }

                            Text {
                                id: nextObjText
                                objectName: "nextTextObj"
                                y: 17
                                height: 22
                                color: "white"
                                text: qsTr("Next")
                                anchors.right: nextObjIcon.left
                                anchors.rightMargin: 5
                                anchors.verticalCenter: parent.verticalCenter
                                visible: true
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignRight
                                font.bold: true
                                font.pixelSize: 11

                                x: 7
                            }

                            Image {
                                id: nextObjIcon
                                x: 46
                                y: 2
                                anchors.right: parent.right
                                anchors.rightMargin: 0
                                anchors.verticalCenterOffset: 0
                                anchors.verticalCenter: parent.verticalCenter
                                sourceSize.height: 24
                                sourceSize.width: 24
                                source: "next.png"
                            }
                        }

                        Rectangle {
                            id: prevObjRect
                            objectName: "prevObj"
                            y: 377
                            width: prevObjText.width + prevObjIcon.width + 5
                            height: 28
                            color: "#00000000"
                            radius: 5
                            anchors.left: parent.left
                            anchors.leftMargin: 8

                            signal prevObjClicked()
                            x: 8

                            Rectangle {
                                id: prevObjForeground
                                radius: 5
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.topMargin: 0
                                opacity: 0
                            }

                            MouseArea {
                                id: prevObjMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: {
                                    prevObjForeground.opacity = 0.1
                                    prevObjText.color = "yellow"
                                }
                                onExited: {
                                    prevObjForeground.opacity = 0.0
                                    prevObjText.color = "white"
                                }
                                onClicked: prevObjRect.prevObjClicked()
                            }

                            Text {
                                id: prevObjText
                                objectName: "prevTextObj"
                                y: 7
                                height: 22
                                color: "#ffffff"
                                text: qsTr("Previous")
                                anchors.left: prevObjIcon.right
                                anchors.leftMargin: 5
                                anchors.verticalCenterOffset: 0
                                font.pixelSize: 11
                                visible: true
                                anchors.verticalCenter: parent.verticalCenter
                                font.bold: true
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                            }

                            Image {
                                id: prevObjIcon
                                x: 0
                                y: 2
                                anchors.verticalCenter: parent.verticalCenter
                                sourceSize.height: 24
                                sourceSize.width: 24
                                source: "previous.png"
                            }
                        }

                        Column {
                            id: detailItemsCol
                            x: 0
                            y: 78
                            width: 200
                            height: 93
                            spacing: 14

                            Text {
                                id: sbText
                                objectName: "sbTextObj"
                                width: 164
                                height: 21
                                color: "#ffffff"
                                text: qsTr("Surface Brightness:")
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                font.pixelSize: 13
                                font.family: "Cantarell"
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                            }

                            Text {
                                id: magText
                                objectName: "magTextObj"
                                width: 164
                                height: 21
                                color: "#ffffff"
                                text: qsTr("Magnitude: ")
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                font.family: "Cantarell"
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignLeft
                                font.pixelSize: 13
                            }

                            Text {
                                id: sizeText
                                objectName: "sizeTextObj"
                                width: 164
                                height: 21
                                color: "#ffffff"
                                text: qsTr("Size: ")
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                font.pixelSize: 13
                                font.family: "Cantarell"
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Column {
                            id: detailsViewButtonsCol
                            x: 208
                            y: 134
                            width: 132
                            height: 52
                            anchors.right: parent.right
                            anchors.rightMargin: 0
                            spacing: 14

                            Text {
                                id: detailsButton
                                objectName: "detailsButtonObj"
                                width: 119
                                height: 16
                                font.underline: true
                                anchors.rightMargin: 10
                                anchors.right: parent.right
                                verticalAlignment: Text.AlignVCenter
                                color: "white"
                                text: qsTr("More object details")
                                font.family: "Cantarell"
                                font.pixelSize: 14

                                signal detailsButtonClicked()
                                x: 0

                                MouseArea {
                                    id: detailsMouseArea
                                    hoverEnabled: true
                                    anchors.fill: parent
                                    onEntered: detailsButton.color = "yellow"
                                    onExited: detailsButton.color = "white"
                                    onClicked: detailsButton.detailsButtonClicked()
                                }
                            }

                            Text {
                                id: slewButton
                                objectName: "slewButtonObj"
                                width: 119
                                height: 16
                                color: "white"
                                text: qsTr("Slew map to object")
                                font.family: "Cantarell"
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                font.underline: true
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 14

                                signal slewButtonClicked()

                                MouseArea {
                                    id: slewObjMouseArea
                                    hoverEnabled: true
                                    anchors.fill: parent
                                    onEntered: slewButton.color = "yellow"
                                    onExited: slewButton.color = "white"
                                    onClicked: slewButton.slewButtonClicked()
                                }
                            }
                        }
                    } //end of detailsView
                } //end of detailsViewContainer

                states: [
                    State {
                        name: "back"
                        PropertyChanges {
                            target: listToDetailsRotation
                            angle: 180
                        }

                        PropertyChanges {
                            target: settingsMouseArea
                            enabled: false
                        }

                        PropertyChanges {
                            target: settingsIcon
                            opacity: 0
                        }

                        PropertyChanges {
                            target: reloadMouseArea
                            enabled: false
                        }

                        PropertyChanges {
                            target: reloadIcon
                            opacity: 0
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
        }//end of viewsContainer
    }//end of base

    Rectangle {
        id: backButton
        x: container.width + 10
        y: 518
        width: leftArrow.width + goBackText.width + 18
        height: 49
        color: "#00000000"
        radius: 5

        Rectangle {
            id: goBackForeground
            anchors.fill: parent
            radius: 5
            opacity: 0.0
        }

        Text {
            id: goBackText
            y: 12
            color: "#f7e808"
            text: qsTr("Back")
            anchors.left: leftArrow.right
            anchors.leftMargin: 7
            anchors.verticalCenterOffset: 0
            anchors.verticalCenter: leftArrow.verticalCenter
            font.family: "Cantarell"
            font.pointSize: 13
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }

        Image {
            id: leftArrow
            y: 9
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.verticalCenterOffset: 0
            anchors.verticalCenter: parent.verticalCenter
            source: "leftArrow.png"
        }

        MouseArea {
            id: backButtonMouseArea
            x: 45
            y: 0
            anchors.fill: backButton
            hoverEnabled: true
            onEntered: goBackForeground.opacity = 0.2
            onExited: goBackForeground.opacity = 0.0
            onClicked: {
                if ( container.state == "soTypeSelected" ) {
                    if ( !skyObjView.flipped ) {
                        container.state = "base"
                    } else if ( skyObjView.flipped ) {
                        skyObjView.flipped = false
                    }
                } else if ( container.state == "dsoTypeSelected" ) {
                    if ( !skyObjView.flipped ) {
                        container.state = "dsoAreaClicked"
                    } else if ( skyObjView.flipped ) {
                        skyObjView.flipped = false
                    }
                }
            }
        }
    }

    Image {
        id: settingsIcon
        objectName: "settingsIconObj"
        x: 9
        y: 529
        width: 28
        height: 28
        anchors.verticalCenterOffset: 0
        anchors.verticalCenter: backButton.verticalCenter
        sourceSize.height: 40
        sourceSize.width: 40
        smooth: true
        fillMode: Image.Stretch
        source: "settingsIcon.png"

        signal settingsIconClicked()

        MouseArea {
            id: settingsMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: settingsForeground.opacity = 0.2
            onExited: settingsForeground.opacity = 0.0
            onClicked: settingsIcon.settingsIconClicked()
        }

        Rectangle {
            id: settingsForeground
            anchors.fill: parent
            opacity: 0.0
            radius: 5
        }
    }

    Image {
        id: reloadIcon
        objectName: "reloadIconObj"
        x: 50
        y: 529
        width: 28
        height: 28
        anchors.verticalCenterOffset: 0
        anchors.verticalCenter: backButton.verticalCenter
        sourceSize.height: 40
        sourceSize.width: 40
        smooth: true
        fillMode: Image.Stretch
        source: "reloadIcon.png"

        signal reloadIconClicked()

        MouseArea {
            id: reloadMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: reloadForeground.opacity = 0.2
            onExited: reloadForeground.opacity = 0.0
            onClicked: reloadIcon.reloadIconClicked()
        }

        Rectangle {
            id: reloadForeground
            radius: 5
            opacity: 0
            anchors.fill: parent
        }
    }

    states: [
        State {
            name: "base"

            PropertyChanges {
                target: galText
                color: "#6b6660"
            }

            PropertyChanges {
                target: nebText
                color: "#6b6660"
            }

            PropertyChanges {
                target: clustText
                color: "#6b6660"
            }
        },
        State {
            name: "planetAreaEntered"

            PropertyChanges {
                target: planetText
                font.pixelSize: 21
                font.bold: true
            }
        },
        State {
            name: "starAreaEntered"

            PropertyChanges {
                target: starText
                font.bold: true
                font.pixelSize: 21
            }
        },
        State {
            name: "conAreaEntered"

            PropertyChanges {
                target: conText
                font.bold: true
                font.pixelSize: 21
            }
        },
        State {
            name: "dsoAreaEntered"

            PropertyChanges {
                target: dsoText
                font.bold: true
                font.pixelSize: 21
            }
        },
        State {
            name: "dsoAreaClicked"
            PropertyChanges {
                target: dsoText
                font.pixelSize: "21"
                font.bold: true
            }

            PropertyChanges {
                target: galRect
                opacity: 1
            }

            PropertyChanges {
                target: nebRect
                opacity: 1
            }

            PropertyChanges {
                target: clustRect
                opacity: 1
            }

            PropertyChanges {
                target: planetRect
                opacity: 0.350
            }

            PropertyChanges {
                target: conRect
                opacity: 0.350
            }

            PropertyChanges {
                target: starRect
                opacity: 0.350
            }

            PropertyChanges {
                target: clustMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: galMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: nebMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: dsoMouseArea
                hoverEnabled: false
            }

            PropertyChanges {
                target: dsoContainer
                y: 160
            }

            PropertyChanges {
                target: galText
                color: "#e4800d"
            }

            PropertyChanges {
                target: clustText
                color: "#e4800d"
            }

            PropertyChanges {
                target: nebText
                color: "#e4800d"
            }
        },
        State {
            name: "galAreaEntered"
            PropertyChanges {
                target: dsoText
                font.pixelSize: "21"
                font.bold: false
            }

            PropertyChanges {
                target: galRect
                opacity: 1
            }

            PropertyChanges {
                target: nebRect
                opacity: 1
            }

            PropertyChanges {
                target: clustRect
                opacity: 1
            }

            PropertyChanges {
                target: planetRect
                opacity: 0.350
            }

            PropertyChanges {
                target: conRect
                opacity: 0.350
            }

            PropertyChanges {
                target: starRect
                opacity: 0.350
            }

            PropertyChanges {
                target: dsoMouseArea
                hoverEnabled: false
            }

            PropertyChanges {
                target: galText
                color: "#e4800d"
                font.bold: true
                font.pixelSize: 21
            }

            PropertyChanges {
                target: dsoContainer
                y: 160
            }

            PropertyChanges {
                target: clustMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: galMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: nebMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: nebText
                color: "#e4800d"
            }

            PropertyChanges {
                target: clustText
                color: "#e4800d"
            }
        },
        State {
            name: "nebAreaEntered"
            PropertyChanges {
                target: dsoText
                font.pixelSize: "21"
                font.bold: false
            }

            PropertyChanges {
                target: galRect
                opacity: 1
            }

            PropertyChanges {
                target: nebRect
                opacity: 1
            }

            PropertyChanges {
                target: clustRect
                opacity: 1
            }

            PropertyChanges {
                target: planetRect
                opacity: 0.350
            }

            PropertyChanges {
                target: conRect
                opacity: 0.350
            }

            PropertyChanges {
                target: starRect
                opacity: 0.350
            }

            PropertyChanges {
                target: dsoMouseArea
                hoverEnabled: false
            }

            PropertyChanges {
                target: nebText
                color: "#e4800d"
                font.bold: true
                font.pixelSize: 21
            }

            PropertyChanges {
                target: dsoContainer
                y: 160
            }

            PropertyChanges {
                target: clustMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: nebMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: galMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: galText
                color: "#e4800d"
            }

            PropertyChanges {
                target: clustText
                color: "#e4800d"
            }
        },
        State {
            name: "clustAreaEntered"
            PropertyChanges {
                target: dsoText
                font.pixelSize: "21"
                font.bold: false
            }

            PropertyChanges {
                target: galRect
                opacity: 1
            }

            PropertyChanges {
                target: nebRect
                opacity: 1
            }

            PropertyChanges {
                target: clustRect
                opacity: 1
            }

            PropertyChanges {
                target: planetRect
                opacity: 0.350
            }

            PropertyChanges {
                target: conRect
                opacity: 0.350
            }

            PropertyChanges {
                target: starRect
                opacity: 0.350
            }

            PropertyChanges {
                target: dsoMouseArea
                hoverEnabled: false
            }

            PropertyChanges {
                target: clustText
                color: "#e4800d"
                font.bold: true
                font.pixelSize: 21
            }

            PropertyChanges {
                target: dsoContainer
                y: 160
            }

            PropertyChanges {
                target: clustMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: galMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: nebMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: nebText
                color: "#e4800d"
            }

            PropertyChanges {
                target: galText
                color: "#e4800d"
            }
        },
        State {
            name: "soTypeSelected"

            PropertyChanges {
                target: viewsRow
                x: -(2*categoryView.width)
                y: 0
                anchors.topMargin: 0
                anchors.bottomMargin: 0
            }

            PropertyChanges {
                target: backButton
                x: container.width - 105
            }
        },
        State {
            name: "dsoTypeSelected"

            PropertyChanges {
                target: viewsRow
                x: -(2*categoryView.width)
                y: 0
                anchors.topMargin: 0
                anchors.bottomMargin: 0
            }

            PropertyChanges {
                target: backButton
                x: container.width - 105
            }

            PropertyChanges {
                target: dsoText
                font.pixelSize: "21"
                font.bold: true
            }

            PropertyChanges {
                target: galRect
                opacity: 1
            }

            PropertyChanges {
                target: nebRect
                opacity: 1
            }

            PropertyChanges {
                target: clustRect
                opacity: 1
            }

            PropertyChanges {
                target: planetRect
                opacity: 0.350
            }

            PropertyChanges {
                target: conRect
                opacity: 0.350
            }

            PropertyChanges {
                target: starRect
                opacity: 0.350
            }

            PropertyChanges {
                target: clustMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: galMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: nebMouseArea
                hoverEnabled: true
                enabled: true
            }

            PropertyChanges {
                target: dsoMouseArea
                hoverEnabled: false
            }

            PropertyChanges {
                target: dsoContainer
                y: 160
            }

            PropertyChanges {
                target: galText
                color: "#e4800d"
            }

            PropertyChanges {
                target: clustText
                color: "#e4800d"
            }

            PropertyChanges {
                target: nebText
                color: "#e4800d"
            }
        }
    ]

    transitions: [
        Transition {
            from: "*"
            to: "planetAreaEntered"
            NumberAnimation { target: planetText; property: "font.pixelSize"; to: 21; duration: 150 }
            NumberAnimation { target: dsoText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: conText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: galText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: nebText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: dsoContainer; property: "y"; duration: 500 }
            NumberAnimation { target: galRect; property: "opacity"; duration: 500 }
            NumberAnimation { target: nebRect; property: "opacity"; duration: 500 }
            NumberAnimation { target: clustRect; property: "opacity"; duration: 500 }
        },
        Transition {
            from: "*"
            to: "starAreaEntered"
            NumberAnimation { target: starText; property: "font.pixelSize"; to: 21; duration: 150 }
            NumberAnimation { target: dsoText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: conText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: galText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: nebText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: dsoContainer; property: "y"; duration: 500 }
            NumberAnimation { target: galRect; property: "opacity"; duration: 500 }
            NumberAnimation { target: nebRect; property: "opacity"; duration: 500 }
            NumberAnimation { target: clustRect; property: "opacity"; duration: 500 }
        },
        Transition {
            from: "*"
            to: "conAreaEntered"
            NumberAnimation { target: conText; property: "font.pixelSize"; to: 21; duration: 150 }
            NumberAnimation { target: dsoText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: conText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: galText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: nebText; property: "font.pixelSize"; duration: 150 }
            NumberAnimation { target: dsoContainer; property: "y"; duration: 500 }
            NumberAnimation { target: galRect; property: "opacity"; duration: 500 }
            NumberAnimation { target: nebRect; property: "opacity"; duration: 500 }
            NumberAnimation { target: clustRect; property: "opacity"; duration: 500 }
        },
        Transition {
            from: "*"
            to: "dsoAreaEntered"
            NumberAnimation { target: dsoText; property: "font.pixelSize"; to: 21; duration: 150 }
        },
        Transition {
            from: "*"
            to: "galAreaEntered"
            NumberAnimation { target: galText; property: "font.pixelSize"; to: 21; duration: 150 }
        },
        Transition {
            from: "*"
            to: "nebAreaEntered"
            NumberAnimation { target: nebText; property: "font.pixelSize"; to: 21; duration: 150 }
        },
        Transition {
            from: "*"
            to: "clustAreaEntered"
            NumberAnimation { target: clustText; property: "font.pixelSize"; to: 21; duration: 150 }
        },
        Transition {
            from: "*"
            to: "dsoAreaClicked"
            NumberAnimation { target: dsoContainer; property: "y"; duration: 200 }
            NumberAnimation { target: galRect; property: "opacity"; duration: 500 }
            NumberAnimation { target: nebRect; property: "opacity"; duration: 500 }
            NumberAnimation { target: clustRect; property: "opacity"; duration: 500 }
        },
        Transition {
            from: "*"
            to: "soTypeSelected"
            NumberAnimation { target: viewsRow; property: "x"; duration: 250; easing.type: Easing.InOutQuad }
            NumberAnimation { target: backButton; property: "x"; duration: 250; easing.type: Easing.InOutQuad }
        },
        Transition {
            from: "*"
            to: "dsoTypeSelected"
            NumberAnimation { target: viewsRow; property: "x"; duration: 250; easing.type: Easing.InOutQuad }
            NumberAnimation { target: backButton; property: "x"; duration: 250; easing.type: Easing.InOutQuad }
        },
        Transition {
            from: "soTypeSelected"
            to: "base"
            NumberAnimation { target: viewsRow; property: "x"; duration: 250; easing.type: Easing.InOutQuad }
            NumberAnimation { target: backButton; property: "x"; duration: 250; easing.type: Easing.InOutQuad }
        },
        Transition {
            from: "dsoTypeSelected"
            to: "dsoAreaClicked"
            NumberAnimation { target: viewsRow; property: "x"; duration: 250; easing.type: Easing.InOutQuad }
            NumberAnimation { target: backButton; property: "x"; duration: 250; easing.type: Easing.InOutQuad }
        }
    ]
}
