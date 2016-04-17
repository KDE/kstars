
// Copyright (C) 2013 Samikshan Bairagya <samikshan@gmail.com>
/***************************************************************************
 *                                                                         *
  *   This program is free software; you can redistribute it and/or modify  *
   *   it under the terms of the GNU General Public License as published by  *
    *   the Free Software Foundation; either version 2 of the License, or     *
     *   (at your option) any later version.                                   *
      *                                                                         *
       ***************************************************************************/
import QtQuick 2.5
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2

Rectangle {
    id: container
    objectName: "containerObj"
    color: "#020518"
    anchors.fill: parent

    property double buttonOpacity: 0.2
    property double categoryTitleOpacity: 0.350

    Text {
        id: title
        x: 9
        y: 34
        color: "#59ad0e"
        text: xi18n("What's Interesting...")
        verticalAlignment: Text.AlignVCenter
        font {
            family: "Cantarell"
            bold: false
            pixelSize:22
        }
    }

    Item {
        id: base
        y: 89
        width: parent.width
        height: 385
        anchors {
            left: parent.left
            leftMargin: 0
            right: parent.right
            rightMargin: 0
        }

        Item {
            id: viewsRow
            objectName: "viewsRowObj"
            width: parent.width
            anchors {
                top: parent.top
                bottom: parent.bottom
            }

            signal categorySelected(int category)

            Item {
                id: categoryView
                x: 0
                y: 31
                width: parent.width
                height: 351

                Rectangle {
                    id: background

                    color: "#00060b"
                    radius: 12
                    anchors {
                        top: parent.top
                        topMargin: 15
                        bottom: parent.bottom
                        bottomMargin: 13
                        right: parent.right
                        rightMargin: 20
                        left: parent.left
                        leftMargin: 20
                    }

                    opacity: 0.500
                    border {
                        width: 4
                        color: "black"
                    }
                }

                Item {
                    id: planetItem
                    width: planetText.width
                    height: planetText.height
                    anchors{
                        verticalCenterOffset: -130
                        horizontalCenterOffset: -30
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: planetText

                        title: xi18n("Planets")
                        anchors.centerIn: parent

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

                Item {
                    id: starItem

                    width: starText.width
                    height: starText.height
                    anchors{
                        verticalCenterOffset: -85
                        horizontalCenterOffset: 87
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: starText

                        title: xi18n("Stars")
                        anchors.centerIn: parent

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

                Item {
                    id: conItem
                    width: conText.width
                    height: conText.height
                    anchors {
                        verticalCenterOffset: -52
                        horizontalCenterOffset: -63
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: conText
                        title: xi18n("Constellations")
                        anchors.centerIn: parent

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

                Item {
                    id: dsoContainer
                    y: 172
                    height: 166
                    anchors {
                        right: parent.right
                        rightMargin: 35
                        left: parent.left
                        leftMargin: 35
                    }

                    Item {
                        id: dsoItem
                        width: dsoText.width
                        height: dsoText.height

                        anchors {
                            verticalCenterOffset: -54
                            horizontalCenterOffset: 0
                            centerIn: parent
                        }

                        CategoryTitle {
                            id: dsoText

                            title: xi18n("Deep-sky Objects")
                            anchors.centerIn: parent

                            MouseArea {
                                id: dsoMouseArea
                                hoverEnabled: true
                                anchors.fill: parent
                                onEntered: container.state = "dsoAreaEntered"
                                onClicked: container.state = "dsoAreaClicked"
                            }
                        }
                    }

                    Item {
                        id: galItem

                        width: galText.width
                        height: galText.height

                        anchors {
                            verticalCenterOffset: -4
                            horizontalCenterOffset: -77
                            centerIn: parent
                        }
                        opacity: categoryTitleOpacity

                        CategoryTitle {
                            id: galText
                            color: disabledColor
                            title: xi18n("Galaxies")
                            anchors {
                                centerIn: parent
                                margins: 0
                            }

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
                        }
                    }

                    Item {
                        id: nebItem

                        width: nebText.width
                        height: nebText.height

                        anchors {
                            verticalCenterOffset: 52
                            horizontalCenterOffset: -17
                            centerIn: parent
                        }
                        opacity: 0.340

                        CategoryTitle {
                            id: nebText
                            color: disabledColor
                            title: xi18n("Nebulae")
                            anchors.centerIn: parent

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
                        }
                    }

                    Item {
                        id: clustItem

                        width: clustText.width
                        height: clustText.height

                        anchors {
                            verticalCenterOffset: 8
                            horizontalCenterOffset: 69
                            centerIn: parent
                        }
                        opacity: categoryTitleOpacity

                        CategoryTitle {
                            id: clustText
                            color: disabledColor
                            title: xi18n("Clusters")
                            anchors.centerIn: parent

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
                        }
                    }
                }
            } //end of categoryView

            Flipable {
                id: skyObjView
                width: parent.width
                height: parent.height
                anchors.leftMargin: categoryView.width

                //anchors.leftMargin: 370
                anchors.left: categoryView.right

                property bool flipped: false

                front: Item {
                    id: soListContainer
                    anchors.fill: parent
                    enabled: !parent.flipped // To hide content of front side on back side

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
                        border {
                            width: 4
                            color: "#000000"
                        }

                        ListView {
                            id: soListView
                            z: 0
                            objectName: "soListObj"
                            anchors.fill: parent

                            signal soListItemClicked(int type, string typeName, int curIndex)
                            clip: true

                            ScrollView {
                                anchors.fill: parent
                                contentItem: soListView
                            }

                            delegate: Item {
                                id: soListItem
                                x: 8
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
                                            soListView.soListItemClicked(
                                                        type, typeName,
                                                        soListView.currentIndex)
                                            skyObjView.flipped = true
                                        }
                                    }
                                }
                            }

                            model: soListModel
                        }
                    }
                }

                back: Item {
                    id: detailsViewContainer
                    width: parent.width
                    height: parent.height
                    enabled: parent.flipped

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
                        border {
                            width: 4
                            color: "#000000"
                        }

                        Text {
                            id: soname
                            objectName: "sonameObj"
                            y: 8
                            width: 273
                            height: 22
                            color: "#ffffff"
                            text: xi18n("text")
                            anchors{
                                left: parent.left
                                leftMargin: 8
                            }
                            font{
                                bold: true
                                pixelSize: 16
                            }
                            verticalAlignment: Text.AlignVCenter
                        }

                        Text {
                            id: posText
                            objectName: "posTextObj"
                            y: 45
                            anchors{
                                right: parent.right
                                rightMargin: 10
                            }
                            width: 320
                            height: 16
                            color: "#f7f7ac"
                            text: xi18n("text")
                            horizontalAlignment: Text.AlignRight
                            font{
                                family: "Cantarell"
                                bold: true
                                pixelSize:11
                            }

                        }

                        Rectangle {
                            id: descTextBox
                            y: 197
                            height: 175
                            color: "#010a14"
                            radius: 10
                            border.width: 0
                            anchors{
                                right: parent.right
                                rightMargin: 4
                                left: parent.left
                                leftMargin: 4
                            }
                            border.color: "#585454"

                            Flickable {
                                id: flickableDescText
                                clip: true
                                flickableDirection: Flickable.VerticalFlick
                                width: parent.width
                                height: parent.height
                                anchors{
                                    top: parent.top
                                    topMargin: 3
                                    bottom: parent.bottom
                                    bottomMargin: 4
                                }

                                contentWidth: parent.width
                                contentHeight: col.height + 4

                                Item {
                                    id: descTextItem
                                    anchors{
                                        top: parent.top
                                        topMargin: 3
                                        left: parent.left
                                        leftMargin: 6
                                        right: parent.right
                                    }
                                    Column {
                                        id: col
                                        width: parent.width
                                        Text {
                                            id: descText
                                            objectName: "descTextObj"
                                            color: "#187988"
                                            text: xi18n("text")
                                            clip: true
                                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                            width: parent.width
                                            font{
                                                family: "Cantarell"
                                                pixelSize: 13
                                            }
                                        }
                                        Text {
                                            id: descSrcText
                                            objectName: "descSrcTextObj"
                                            color: "#18885f"
                                            text: xi18n("Source: ")
                                            clip: true
                                            horizontalAlignment: Text.AlignRight
                                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                            width: parent.width
                                            onLinkActivated: Qt.openUrlExternally(link)
                                            font{
                                                family: "Cantarell"
                                                pixelSize: 13
                                                italic: true
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            id: nextObjRect
                            objectName: "nextObj"
                            width: nextObjText.width + nextObjIcon.width + 10
                            height: 28
                            anchors {
                                right: parent.right
                                rightMargin: 10
                                bottom: parent.bottom
                                bottomMargin: 10
                            }

                            signal nextObjClicked

                            Rectangle {
                                id: nextObjForeground
                                radius: 5
                                anchors.fill: parent
                                opacity: 0
                            }

                            MouseArea {
                                id: nextObjMouseArea

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

                                height: 22
                                color: "white"
                                text: xi18n("Next")
                                anchors{
                                    right: nextObjIcon.left
                                    rightMargin: 5
                                    verticalCenter: parent.verticalCenter
                                }
                                visible: true
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignRight
                                font{
                                    bold: true
                                    pixelSize:11
                                }
                            }

                            Image {
                                id: nextObjIcon

                                anchors{
                                    right: parent.right
                                    verticalCenter: parent.verticalCenter
                                }
                                sourceSize {
                                    height: 24
                                    width: 24
                                }
                                source: "next.png"
                            }
                        }

                        Item {
                            id: prevObjRect
                            objectName: "prevObj"

                            width: prevObjText.width + prevObjIcon.width + 10
                            height: 28
                            anchors {
                                left: parent.left
                                leftMargin: 10
                                bottom: parent.bottom
                                bottomMargin: 10
                            }

                            signal prevObjClicked

                            Rectangle {
                                id: prevObjForeground
                                radius: 5
                                anchors.fill: parent
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

                                height: 22
                                color: "#ffffff"
                                text: xi18n("Previous")
                                anchors{
                                    left: prevObjIcon.right
                                    leftMargin: 5
                                    verticalCenter: parent.verticalCenter
                                }
                                visible: true
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                font{
                                    pixelSize: 11
                                    bold: true
                                }
                            }

                            Image {
                                id: prevObjIcon
                                anchors.verticalCenter: parent.verticalCenter
                                sourceSize{
                                    height: 24
                                    width: 24
                                }
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

                            DetailsItem {
                                id: sbText
                                objectName: "sbTextObj"
                                text: xi18n("Surface Brightness:")
                            }

                            DetailsItem {
                                id: magText
                                objectName: "magTextObj"
                                text: xi18n("Magnitude: ")
                            }

                            DetailsItem {
                                id: sizeText
                                objectName: "sizeTextObj"
                                text: xi18n("Size: ")
                            }
                        }

                        Column {
                            id: detailsViewButtonsCol
                            y: 134
                            //width: 132
                            //height: 52
                            anchors{
                                right: parent.right
                                rightMargin: 10
                            }
                            spacing: 14

                            Text {
                                id: detailsButton
                                objectName: "detailsButtonObj"

                                verticalAlignment: Text.AlignVCenter
                                color: "white"
                                text: xi18n("More object details")
                                font {
                                    underline: true
                                    family: "Cantarell"
                                    pixelSize: 14
                                }

                                signal detailsButtonClicked

                                MouseArea {
                                    id: detailsMouseArea
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    anchors.fill: parent
                                    onEntered: detailsButton.color = "yellow"
                                    onExited: detailsButton.color = "white"
                                    onClicked: detailsButton.detailsButtonClicked()
                                }
                            }

                            Text {
                                id: slewButton
                                objectName: "slewButtonObj"

                                verticalAlignment: Text.AlignVCenter
                                color: "white"
                                text: xi18n("Slew map to object")
                                font {
                                    underline: true
                                    family: "Cantarell"
                                    pixelSize: 14
                                }

                                signal slewButtonClicked

                                MouseArea {
                                    id: slewObjMouseArea
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
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
                        NumberAnimation {
                            target: listToDetailsRotation
                            property: "angle"
                            duration: 400
                        }
                    }
                ]

                transform: Rotation {
                    id: listToDetailsRotation
                    origin.x: container.width / 2
                    axis.y: 1
                    axis.z: 0
                }
            } //end of skyObjView
        } //end of viewsContainer
    } //end of base

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
            text: xi18n("Back")
            anchors {
                left: leftArrow.right
                leftMargin: 7
                verticalCenterOffset: 0
                verticalCenter: leftArrow.verticalCenter
            }
            font{
                family: "Cantarell"
                pointSize: 13
            }
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }

        Image {
            id: leftArrow
            y: 9
            anchors{
                left: parent.left
                leftMargin: 4
                verticalCenterOffset: 0
                verticalCenter: parent.verticalCenter
            }
            source: "leftArrow.png"
        }

        MouseArea {
            id: backButtonMouseArea
            anchors.fill: backButton
            hoverEnabled: true
            onEntered: goBackForeground.opacity = buttonOpacity
            onExited: goBackForeground.opacity = 0.0
            onClicked: {
                if (container.state == "soTypeSelected") {
                    if (!skyObjView.flipped) {
                        container.state = "base"
                    } else if (skyObjView.flipped) {
                        skyObjView.flipped = false
                    }
                } else if (container.state == "dsoTypeSelected") {
                    if (!skyObjView.flipped) {
                        container.state = "dsoAreaClicked"
                    } else if (skyObjView.flipped) {
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
        anchors{
            verticalCenterOffset: 0
            verticalCenter: backButton.verticalCenter
        }
        sourceSize{
            height: 40
            width: 40
        }
        smooth: true
        fillMode: Image.Stretch
        source: "settingsIcon.png"

        signal settingsIconClicked

        MouseArea {
            id: settingsMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: settingsForeground.opacity = buttonOpacity
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
        anchors{
            verticalCenterOffset: 0
            verticalCenter: backButton.verticalCenter
        }
        sourceSize{
            height: 40
            width: 40
        }
        smooth: true
        fillMode: Image.Stretch
        source: "reloadIcon.png"

        signal reloadIconClicked

        MouseArea {
            id: reloadMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: reloadForeground.opacity = buttonOpacity
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
                color: disabledColor
            }

            PropertyChanges {
                target: nebText
                color: disabledColor
            }

            PropertyChanges {
                target: clustText
                color: disabledColor
            }
        },
        State {
            name: "planetAreaEntered"

            PropertyChanges {
                target: planetText
                state: "selected"
            }
        },
        State {
            name: "starAreaEntered"

            PropertyChanges {
                target: starText
                state: "selected"
            }
        },
        State {
            name: "conAreaEntered"

            PropertyChanges {
                target: conText
                state: "selected"
            }
        },
        State {
            name: "dsoAreaEntered"

            PropertyChanges {
                target: dsoText
                state:"selected"
            }
        },
        State {
            name: "dsoAreaClicked"

            PropertyChanges {
                target: dsoText
                state:"selected"
            }

            PropertyChanges {
                target: galItem
                opacity: 1
            }

            PropertyChanges {
                target: nebItem
                opacity: 1
            }

            PropertyChanges {
                target: clustItem
                opacity: 1
            }

            PropertyChanges {
                target: planetItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: conItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: starItem
                opacity: categoryTitleOpacity
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
                color: activeColor
            }

            PropertyChanges {
                target: clustText
                color: activeColor
            }

            PropertyChanges {
                target: nebText
                color: activeColor
            }
        },
        State {
            name: "galAreaEntered"
            PropertyChanges {
                target: dsoText
                state: "selectedNoBold"
            }

            PropertyChanges {
                target: galItem
                opacity: 1
            }

            PropertyChanges {
                target: nebItem
                opacity: 1
            }

            PropertyChanges {
                target: clustItem
                opacity: 1
            }

            PropertyChanges {
                target: planetItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: conItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: starItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: dsoMouseArea
                hoverEnabled: false
            }

            PropertyChanges {
                target: galText
                state: "selected"
                color: activeColor
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
                color: activeColor
            }

            PropertyChanges {
                target: clustText
                color: activeColor
            }
        },
        State {
            name: "nebAreaEntered"
            PropertyChanges {
                target: dsoText
                state: "selectedNoBold"
            }

            PropertyChanges {
                target: galItem
                opacity: 1
            }

            PropertyChanges {
                target: nebItem
                opacity: 1
            }

            PropertyChanges {
                target: clustItem
                opacity: 1
            }

            PropertyChanges {
                target: planetItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: conItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: starItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: dsoMouseArea
                hoverEnabled: false
            }

            PropertyChanges {
                target: nebText
                state: "selected"
                color: activeColor
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
                color: activeColor
            }

            PropertyChanges {
                target: clustText
                color: activeColor
            }
        },
        State {
            name: "clustAreaEntered"
            PropertyChanges {
                target: dsoText
                state: "selectedNoBold"
            }

            PropertyChanges {
                target: galItem
                opacity: 1
            }

            PropertyChanges {
                target: nebItem
                opacity: 1
            }

            PropertyChanges {
                target: clustItem
                opacity: 1
            }

            PropertyChanges {
                target: planetItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: conItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: starItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: dsoMouseArea
                hoverEnabled: false
            }

            PropertyChanges {
                target: clustText
                state: "selected"
                color: activeColor
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
                color: activeColor
            }

            PropertyChanges {
                target: galText
                color: activeColor
            }
        },
        State {
            name: "soTypeSelected"

            PropertyChanges {
                target: viewsRow
                x: -(2 * categoryView.width)
                y: 0
                anchors{
                    topMargin: 0
                    bottomMargin: 0
                }
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
                x: -(2 * categoryView.width)
                y: 0
                anchors{
                    topMargin: 0
                    bottomMargin: 0
                }
            }

            PropertyChanges {
                target: backButton
                x: container.width - 105
            }

            PropertyChanges {
                target: dsoText
                state:"selectedNoBold"
            }

            PropertyChanges {
                target: galItem
                opacity: 1
            }

            PropertyChanges {
                target: nebItem
                opacity: 1
            }

            PropertyChanges {
                target: clustItem
                opacity: 1
            }

            PropertyChanges {
                target: planetItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: conItem
                opacity: categoryTitleOpacity
            }

            PropertyChanges {
                target: starItem
                opacity: categoryTitleOpacity
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
                color: activeColor
            }

            PropertyChanges {
                target: clustText
                color: activeColor
            }

            PropertyChanges {
                target: nebText
                color: activeColor
            }
        }
    ]

    transitions: [
        Transition {
            from: "*"
            to: "planetAreaEntered"

            NumberAnimation {
                target: dsoContainer
                property: "y"
                duration: 500
            }
            NumberAnimation {
                target: galItem
                property: "opacity"
                duration: 500
            }
            NumberAnimation {
                target: nebItem
                property: "opacity"
                duration: 500
            }
            NumberAnimation {
                target: clustItem
                property: "opacity"
                duration: 500
            }
        },
        Transition {
            from: "*"
            to: "starAreaEntered"

            NumberAnimation {
                target: dsoContainer
                property: "y"
                duration: 500
            }
            NumberAnimation {
                target: galItem
                property: "opacity"
                duration: 500
            }
            NumberAnimation {
                target: nebItem
                property: "opacity"
                duration: 500
            }
            NumberAnimation {
                target: clustItem
                property: "opacity"
                duration: 500
            }
        },
        Transition {
            from: "*"
            to: "conAreaEntered"

            NumberAnimation {
                target: dsoContainer
                property: "y"
                duration: 500
            }
            NumberAnimation {
                target: galItem
                property: "opacity"
                duration: 500
            }
            NumberAnimation {
                target: nebItem
                property: "opacity"
                duration: 500
            }
            NumberAnimation {
                target: clustItem
                property: "opacity"
                duration: 500
            }
        },
        Transition {
            from: "*"
            to: "dsoAreaClicked"
            NumberAnimation {
                target: dsoContainer
                property: "y"
                duration: 200
            }
            NumberAnimation {
                target: galItem
                property: "opacity"
                duration: 500
            }
            NumberAnimation {
                target: nebItem
                property: "opacity"
                duration: 500
            }
            NumberAnimation {
                target: clustItem
                property: "opacity"
                duration: 500
            }
        },
        Transition {
            from: "*"
            to: "soTypeSelected"
            NumberAnimation {
                target: viewsRow
                property: "x"
                duration: 250
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: backButton
                property: "x"
                duration: 250
                easing.type: Easing.InOutQuad
            }
        },
        Transition {
            from: "*"
            to: "dsoTypeSelected"
            NumberAnimation {
                target: viewsRow
                property: "x"
                duration: 250
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: backButton
                property: "x"
                duration: 250
                easing.type: Easing.InOutQuad
            }
        },
        Transition {
            from: "soTypeSelected"
            to: "base"
            NumberAnimation {
                target: viewsRow
                property: "x"
                duration: 250
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: backButton
                property: "x"
                duration: 250
                easing.type: Easing.InOutQuad
            }
        },
        Transition {
            from: "dsoTypeSelected"
            to: "dsoAreaClicked"
            NumberAnimation {
                target: viewsRow
                property: "x"
                duration: 250
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: backButton
                property: "x"
                duration: 250
                easing.type: Easing.InOutQuad
            }
        }
    ]
}
