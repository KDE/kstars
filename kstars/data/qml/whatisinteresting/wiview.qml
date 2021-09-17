// SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

// based on work of:
// SPDX-FileCopyrightText: 2013 Samikshan Bairagya <samikshan@gmail.com>

// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.5
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

Rectangle {
    id: container
    objectName: "containerObj"
    color: "#020518"
    anchors.fill: parent

    property double buttonOpacity: 0.2
    property double categoryTitleOpacity: 0.350

    ProgressBar {
        id: progress
        objectName: "progressBar"
        width: container.width
        value: 0.10
    }

    Text {
        id: title
        x: 9
        y: 20
        color: "#59ad0e"
        text: xi18n("What's Interesting...")
        verticalAlignment: Text.AlignVCenter
        font {
            family: "Cantarell"
            bold: false
            pixelSize:22
        }
    }

    Text {
        id: catTitle
        objectName: "categoryTitle"
        x: 30
        y: 50
        color: "white"
        text: ""
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
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
        height: parent.height
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

            signal categorySelected(string category)
            signal inspectSkyObject(string name);

            Item {
                id: categoryView
                width: parent.width
                height: parent.height - 150

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
                    id: nakedEyeItem
                    width: nakedEyeText.width
                    height: nakedEyeText.height
                    anchors{
                        verticalCenterOffset: -250
                        horizontalCenterOffset: 0
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: nakedEyeText
                        color: "yellow"
                        title: xi18n("Naked-Eye Objects")
                        anchors.centerIn: parent
                    }
                }

                Item {
                    id: sunItem
                    width: sunText.width
                    height: sunText.height
                    anchors{
                        verticalCenterOffset: -210
                        horizontalCenterOffset: -50
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: sunText

                        title: xi18n("Sun")
                        anchors.centerIn: parent

                        MouseArea {
                            id: sunMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: sunText.state = "selected"
                            onExited: sunText.state = ""
                            onClicked: {
                                viewsRow.inspectSkyObject("Sun")
                                catTitle.text = xi18n("Sun")
                                container.state = "singleItemSelected"
                            }
                        }
                    }
                }

                Item {
                    id: moonItem
                    width: moonText.width
                    height: moonText.height
                    anchors{
                        verticalCenterOffset: -210
                        horizontalCenterOffset: 50
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: moonText

                        title: xi18n("Moon")
                        anchors.centerIn: parent

                        MouseArea {
                            id: moonMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: moonText.state = "selected"
                            onExited: moonText.state = ""
                            onClicked: {
                                viewsRow.inspectSkyObject("Moon")
                                catTitle.text = xi18n("Moon")
                                container.state = "singleItemSelected"
                            }
                        }
                    }
                }



                Item {
                    id: planetItem
                    width: planetText.width
                    height: planetText.height
                    anchors{
                        verticalCenterOffset: -170
                        horizontalCenterOffset: -50
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
                            onEntered: planetText.state = "selected"
                            onExited: planetText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("planets")
                                catTitle.text = xi18n("Planets")
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: satelliteItem
                    width: satelliteText.width
                    height: satelliteText.height
                    anchors {
                        verticalCenterOffset: -170
                        horizontalCenterOffset: 50
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: satelliteText
                        title: xi18n("Satellites")
                        anchors.centerIn: parent

                        MouseArea {
                            id: satelliteMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: satelliteText.state = "selected"
                            onExited: satelliteText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("satellites")
                                catTitle.text = xi18n("Satellites")
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: starItem

                    width: starText.width
                    height: starText.height
                    anchors{
                        verticalCenterOffset: -130
                        horizontalCenterOffset: -50
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
                            onEntered: starText.state = "selected"
                            onExited: starText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("stars")
                                catTitle.text = xi18n("Stars")
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: conItem
                    width: conText.width
                    height: conText.height
                    anchors {
                        verticalCenterOffset: -130
                        horizontalCenterOffset: 50
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
                            onEntered: conText.state = "selected"
                            onExited: conText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("constellations")
                                catTitle.text = xi18n("Constellations")
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: dsoItem
                    width: dsoText.width
                    height: dsoText.height

                    anchors {
                        verticalCenterOffset: -90
                        horizontalCenterOffset: 0
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: dsoText
                        color: "yellow"
                        title: xi18n("Deep-sky Objects")
                        anchors.centerIn: parent
                    }
                }

                Item {
                    id: asteroidItem

                    width: asteroidText.width
                    height: asteroidText.height
                    anchors{
                        verticalCenterOffset: -50
                        horizontalCenterOffset: -50
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: asteroidText

                        title: xi18n("Asteroids")
                        anchors.centerIn: parent

                        MouseArea {
                            id: asteroidMouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            onEntered: asteroidText.state = "selected"
                            onExited: asteroidText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("asteroids")
                                catTitle.text = xi18n("Asteroids")
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: cometItem

                    width: cometText.width
                    height: cometText.height
                    anchors{
                        verticalCenterOffset: -50
                        horizontalCenterOffset: 50
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: cometText

                        title: xi18n("Comets")
                        anchors.centerIn: parent

                        MouseArea {
                            id: cometMouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            onEntered: cometText.state = "selected"
                            onExited: cometText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("comets")
                                catTitle.text = xi18n("Comets")
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: galItem

                    width: galText.width
                    height: galText.height

                    anchors {
                        verticalCenterOffset: -10
                        horizontalCenterOffset: -50
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: galText
                        title: xi18n("Galaxies")
                        anchors {
                            centerIn: parent
                            margins: 0
                        }

                        MouseArea {
                            id: galMouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            onEntered: galText.state = "selected"
                            onExited: galText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("galaxies")
                                catTitle.text = xi18n("Galaxies")
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: nebItem

                    width: nebText.width
                    height: nebText.height

                    anchors {
                        verticalCenterOffset: -10
                        horizontalCenterOffset: 50
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: nebText
                        title: xi18n("Nebulae")
                        anchors.centerIn: parent

                        MouseArea {
                            id: nebMouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            onEntered: nebText.state = "selected"
                            onExited: nebText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("nebulas")
                                catTitle.text = xi18n("Nebulae")
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: clustItem

                    width: clustText.width
                    height: clustText.height

                    anchors {
                        verticalCenterOffset: 30
                        horizontalCenterOffset: -75
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: clustText
                        title: xi18n("Clusters")
                        anchors.centerIn: parent

                        MouseArea {
                            id: clustMouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            onEntered: clustText.state = "selected"
                            onExited: clustText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("clusters")
                                catTitle.text = xi18n("Clusters")
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: superItem

                    width: superText.width
                    height: superText.height

                    anchors {
                        verticalCenterOffset: 30
                        horizontalCenterOffset: 75
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: superText
                        title: xi18n("Supernovae")
                        anchors.centerIn: parent

                        MouseArea {
                            id: superMouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            onEntered: superText.state = "selected"
                            onExited: superText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("supernovas")
                                catTitle.text = xi18n("Supernovae")
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: catalogsItem
                    width: catalogText.width
                    height: catalogText.height
                    anchors{
                        verticalCenterOffset: 70
                        horizontalCenterOffset: 0
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: catalogText
                        color: "yellow"
                        title: xi18n("Explore Catalogs")
                        anchors.centerIn: parent
                    }
                }

                Item {
                    id: messierItem

                    width: messierText.width
                    height: messierText.height

                    anchors {
                        verticalCenterOffset: 110
                        horizontalCenterOffset: 0
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: messierText
                        title: xi18n("Messier Catalog")
                        anchors.centerIn: parent

                        MouseArea {
                            id: messierMouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            onEntered: messierText.state = "selected"
                            onExited: messierText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("messier")
                                catTitle.text = "Messier Catalog"
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: ngcItem

                    width: ngcText.width
                    height: ngcText.height

                    anchors {
                        verticalCenterOffset: 150
                        horizontalCenterOffset: 0
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: ngcText
                        title: xi18n("NGC Catalog")
                        anchors.centerIn: parent

                        MouseArea {
                            id: ngcMouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            onEntered: ngcText.state = "selected"
                            onExited: ngcText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("ngc")
                                catTitle.text = "NGC Catalog"
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: icItem

                    width: icText.width
                    height: icText.height

                    anchors {
                        verticalCenterOffset: 190
                        horizontalCenterOffset: 0
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: icText
                        title: xi18n("IC Catalog")
                        anchors.centerIn: parent

                        MouseArea {
                            id: icMouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            onEntered: icText.state = "selected"
                            onExited: icText.state = ""
                            onClicked: {
                                viewsRow.categorySelected("ic")
                                catTitle.text = "IC Catalog"
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

                Item {
                    id: sh2Item

                    width: sh2Text.width
                    height: sh2Text.height

                    anchors {
                        verticalCenterOffset: 230
                        horizontalCenterOffset: 0
                        centerIn: parent
                    }

                    CategoryTitle {
                        id: sh2Text
                        title: xi18n("Sharpless Catalog")
                        anchors.centerIn: parent

                        MouseArea {
                            id: sh2MouseArea
                            hoverEnabled: true
                            anchors.fill: parent
                            onEntered: sh2Text.state = "selected"
                            onExited: sh2Text.state = ""
                            onClicked: {
                                viewsRow.categorySelected("sharpless")
                                catTitle.text = "Sharpless Catalog"
                                container.state = "objectFromListSelected"
                            }
                        }
                    }
                }

            } //end of categoryView

            Flipable {
                id: skyObjView
                objectName: "skyObjView"
                width: parent.width
                height: parent.height - 150
                anchors.leftMargin: categoryView.width
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
                        anchors{
                            top: soListContainer.top
                            bottom: soListContainer.bottom
                            left: soListContainer.left
                            right: soListContainer.right
                        }
                        color: "transparent"
                        radius: 12
                        border {
                            width: 4
                            color: "#000000"
                        }

                        Flickable {
                            id: flickable
                            anchors.fill: parent
                            clip: true
                            flickableDirection: Flickable.VerticalFlick


                            ListView {
                                id: soListView
                                z: 0
                                objectName: "soListObj"
                                anchors.fill: parent

                                signal soListItemClicked(int curIndex)
                                clip: true

                                highlightMoveDuration: 1

                                model: soListModel

                                Rectangle{
                                    id: soListEmptyMessage
                                    objectName: "soListEmptyMessage"
                                    color: "#00060b"
                                    anchors.fill: parent
                                    Text{
                                        anchors.fill: parent
                                        text: xi18n("No Items to display")
                                        verticalAlignment: Text.AlignVCenter
                                        horizontalAlignment: Text.AlignHCenter
                                        color: "white"
                                        font{
                                            family: "Arial"
                                            pointSize: 20
                                        }
                                    }
                                    visible: (soListView.count > 0 || container.state == "singleItemSelected") ? false : true
                                }

                                Rectangle {
                                    id: scrollbar
                                    anchors.right: soListView.right
                                    y: soListView.visibleArea.yPosition * soListView.height
                                    width: 10
                                    height: (soListView.visibleArea.heightRatio * soListView.height > 10) ? soListView.visibleArea.heightRatio * soListView.height : 10
                                    color: "blue"
                                    MouseArea {
                                        id: dragScrollBar
                                        drag.target: scrollbar
                                        drag.axis: Drag.YAxis
                                        drag.minimumY: 0
                                        drag.maximumY: soListView.height - scrollbar.height
                                        anchors.fill: parent
                                        enabled: true
                                        onPositionChanged: {
                                            soListView.contentY = scrollbar.y / soListView.height * soListView.contentHeight
                                        }

                                    }//Mousearea
                                }

                                delegate: Item {
                                    id: soListItem
                                    x: 8
                                    width: parent.width
                                    height: (dispSummary.height >= 130) ? dispSummary.height + 20 : 160

                                    Rectangle{
                                        id: summaryBackground
                                        color: (mouseListArea.containsMouse||mouseImgArea.containsMouse||mouseTextArea.containsMouse) ? "#030723" : "transparent"
                                        width: parent.width
                                        height: parent.height
                                        MouseArea {
                                            id: mouseListArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: {
                                                soListView.currentIndex = index
                                                soListView.soListItemClicked(soListView.currentIndex)
                                                skyObjView.flipped = true
                                            }
                                        }//Mousearea
                                        Text {
                                            id: dispSummary
                                            objectName: dispObjSummary
                                            text: dispObjSummary
                                            textFormat: Text.RichText
                                            x: image.width + 5
                                            width: parent.width - image.width - 30
                                            color: (nightVision.state == "active" && soListItem.ListView.isCurrentItem) ? "#F89404" : (nightVision.state == "active") ? "red" : (soListItem.ListView.isCurrentItem) ? "white" : (mouseListArea.containsMouse||mouseImgArea.containsMouse||mouseTextArea.containsMouse) ? "yellow" : "gray"

                                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                            font{
                                                family: "Arial"
                                                pixelSize: 13
                                            }

                                            MouseArea {
                                                id: mouseTextArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                onClicked: {
                                                    soListView.currentIndex = index
                                                    soListView.soListItemClicked(soListView.currentIndex)
                                                    skyObjView.flipped = true
                                                }
                                            }//Mousearea
                                        }
                                    }
                                    Image {
                                        id: image
                                        width: 150
                                        height: parent.height
                                        fillMode: Image.PreserveAspectFit
                                        source: imageSource
                                        MouseArea {
                                            id: mouseImgArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: {
                                                soListView.currentIndex = index
                                                soListView.soListItemClicked(soListView.currentIndex)
                                            }
                                        }//Mousearea
                                    }
                                    Text {
                                        id: dispText
                                        objectName: dispName
                                        text: dispName
                                        color: (nightVision.state == "active" && soListItem.ListView.isCurrentItem) ? "#F89404" : (nightVision.state == "active") ? "red" : (mouseListArea.containsMouse||mouseImgArea.containsMouse||mouseTextArea.containsMouse) ? "yellow" : "white"

                                        font.bold: true
                                    }
                                }//soListItem
                            }//soListView
                        }//Flickable
                    }//soListViewContainer
                }//Front, soListContainer

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
                        height: parent.height
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
                            width: parent.width
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
                            y: parent.height - 50
                            anchors{
                                right: parent.right
                                rightMargin: 10
                            }
                            textFormat: Text.RichText
                            width: parent.width
                            height: 16
                            text: xi18n("text")
                            horizontalAlignment: Text.AlignRight
                            font{
                                family: "Arial"
                                bold: true
                                pixelSize:11
                            }

                        }
                        Column {
                            id: detailItemsCol
                            x: 150
                            y: 80
                            width: parent.width
                            height: 93
                            spacing: 14

                            DetailsItem {
                                id: detailsText
                                textFormat: Text.RichText
                                objectName: "detailsTextObj"

                            }

                        }

                        Column {
                            id: detailsViewButtonsCol
                            y: 50
                            anchors {
                                left: parent.left
                                leftMargin: 10
                            }

                            spacing: 14

                            Text {
                                id: detailsButton
                                objectName: "detailsButtonObj"

                                verticalAlignment: Text.AlignVCenter
                                color: "white"
                                text: xi18n("More Details")
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
                                    onEntered: detailsButton.color = (nightVision.state == "active") ? "red" : "yellow"
                                    onExited: detailsButton.color = (nightVision.state == "active") ? "red" : "white"
                                    onClicked: detailsButton.detailsButtonClicked()
                                }
                            }

                            Text {
                                id: centerButton
                                objectName: "centerButtonObj"

                                verticalAlignment: Text.AlignVCenter
                                color: "white"
                                text: xi18n("Center in Map \n")
                                font {
                                    underline: true
                                    family: "Arial"
                                    pixelSize: 14
                                }

                                signal centerButtonClicked

                                MouseArea {
                                    id: centerObjMouseArea
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    anchors.fill: parent
                                    onEntered: centerButton.color = (nightVision.state == "active") ? "red" : "yellow"
                                    onExited: centerButton.color = (nightVision.state == "active") ? "red" : "white"
                                    onClicked: centerButton.centerButtonClicked()
                                }

                                Text {
                                    text: xi18n(" Auto     Track   ")
                                    color: "white"
                                    font {
                                        family: "Arial"
                                        pixelSize: 14
                                    }
                                    y: 15
                                }

                                CheckBox {
                                    id: autoCenter
                                    objectName: "autoCenterCheckbox"
                                    x: 37
                                    y: 15
                                    checked: true
                                }

                                CheckBox {
                                    id: autoTrack
                                    objectName: "autoTrackCheckbox"
                                    x: 97
                                    y: 15
                                    checked: false
                                    onClicked: centerButton.centerButtonClicked()
                                }

                            }


                            Text {
                                id: slewTelescopeButton
                                objectName: "slewTelescopeButtonObj"

                                verticalAlignment: Text.AlignVCenter
                                color: "white"
                                text: xi18n("Slew Telescope")
                                font {
                                    underline: true
                                    family: "Cantarell"
                                    pixelSize: 14
                                }

                                signal slewTelescopeButtonClicked

                                MouseArea {
                                    id: slewTelescopeObjMouseArea
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    anchors.fill: parent
                                    onEntered: slewTelescopeButton.color = (nightVision.state == "active") ? "red" : "yellow"
                                    onExited: slewTelescopeButton.color = (nightVision.state == "active") ? "red" : "white"
                                    onClicked: slewTelescopeButton.slewTelescopeButtonClicked()
                                }
                            }
                        }
                        TabView {
                            id: tabbedView
                            y: 170
                            width: parent.width
                            height: parent.height - 170 - 50
                            frameVisible: false

                            property Component nightTabs: TabViewStyle {
                                tabsAlignment: Qt.AlignHCenter
                                frameOverlap: 1
                                tab: Rectangle {
                                    border.color: "black"
                                    implicitWidth: 150
                                    implicitHeight: 30
                                    color: "red"
                                    Text {
                                        id: text
                                        anchors.centerIn: parent
                                        text: styleData.title
                                        color: styleData.selected ? "white" : "black"
                                    }
                                }
                            }

                            Tab {
                                title: xi18n("Object Information")

                                Rectangle {
                                    id: descTextBox
                                    height: parent.height
                                    width:  parent.width
                                    color: "#010a14"
                                    radius: 10
                                    border.width: 0
                                    anchors{
                                        top: parent.top
                                        left: parent.left
                                        leftMargin: 4
                                        right: parent.right
                                        rightMargin: 4
                                    }
                                    border.color: "#585454"

                                    Flickable {
                                        id: flickableDescText
                                        clip: true
                                        flickableDirection: Flickable.VerticalFlick
                                        width: parent.width
                                        height: parent.height - 10
                                        anchors{
                                            top: parent.top
                                            topMargin: 20
                                            bottom: parent.bottom
                                            bottomMargin: 4
                                            left: parent.left
                                            leftMargin: 10
                                            right: parent.right
                                            rightMargin: 10
                                        }
                                        contentWidth:  parent.width
                                        contentHeight: col.height + 4
                                        Item {
                                            id: descTextItem
                                            anchors.fill: parent
                                            Column {
                                                id: col
                                                width: parent.width
                                                Image {
                                                    id: detailImg
                                                    width: parent.width - 20
                                                    anchors{
                                                        right: parent.right
                                                    }
                                                    objectName: "detailImage"
                                                    property string refreshableSource
                                                    fillMode: Image.PreserveAspectFit
                                                    source: refreshableSource
                                                }
                                                Text {
                                                    id: descText
                                                    objectName: "descTextObj"
                                                    color: "white"
                                                    text: xi18n("text")
                                                    clip: true
                                                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                                    width: parent.width - 20
                                                    textFormat: Text.RichText
                                                    font{
                                                        family: "Arial"
                                                        pixelSize: 13
                                                    }
                                                    onLinkActivated: Qt.openUrlExternally(link)
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        acceptedButtons: Qt.NoButton // we don't want to eat clicks on the Text
                                                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                                                    }
                                                }
                                            } //column
                                        } //item
                                    } //flickable
                                } //rectangle
                            } //tab

                            Tab {
                                id: infoBoxTab
                                title: xi18n("Wikipedia Infotext")
                                active: true
                                Rectangle {
                                    id: descTextBox2
                                    color: "#010a14"
                                    radius: 10
                                    border.width: 0
                                    states: [
                                        State {
                                            name: "outOfTab"
                                            when: ( (container.state == "singleItemSelected" && detailsView.width >= 600)||(container.state != "singleItemSelected" && detailsView.width >= 600 && detailsView.width < 900))
                                            PropertyChanges{target:descTextBox2; parent: detailsView}
                                            PropertyChanges{target:descTextBox2; width: detailsView.width / 2}
                                            PropertyChanges{target:descTextBox2; anchors{
                                                top: detailsView.top
                                                topMargin: 4
                                                bottom: posText.top
                                                left: tabbedView.right
                                                right: detailsView.right
                                            }
                                                           }
                                            PropertyChanges{target:tabbedView; currentIndex: 0}
                                            PropertyChanges{target:tabbedView; tabsVisible: false}
                                            PropertyChanges{target:tabbedView; width: detailsView.width / 2}
                                        },
                                        State {
                                            name: "includeList"
                                            when: (detailsView.width >= 900 && container.state!="singleItemSelected")
                                            PropertyChanges{target: soListViewContainer; parent: detailsView}
                                            PropertyChanges{target: soListViewContainer; anchors{
                                                top: detailsView.top
                                                bottom: posText.top
                                                left: detailsView.left
                                                right: tabbedView.left
                                            }
                                                           }
                                            PropertyChanges{target:descTextBox2; parent: detailsView}
                                            PropertyChanges{target:descTextBox2; width: detailsView.width / 3}
                                            PropertyChanges{target:descTextBox2; anchors{
                                                top: detailsView.top
                                                topMargin: 4
                                                bottom: posText.top
                                                left: tabbedView.right
                                                right: detailsView.right
                                            }
                                                           }
                                            PropertyChanges{target:soListViewContainer; width: detailsView.width / 3}
                                            PropertyChanges{target:tabbedView; x: detailsView.width / 3}
                                            PropertyChanges{target:detailsViewButtonsCol; anchors.left:  soListViewContainer.right}
                                            PropertyChanges{target:soname; anchors.left:  soListViewContainer.right}
                                            PropertyChanges{target:detailItemsCol; x: 150 + detailsView.width / 3}
                                            PropertyChanges{target:tabbedView; width: detailsView.width / 3}
                                            PropertyChanges{target:tabbedView; currentIndex: 0}
                                            PropertyChanges{target:tabbedView; tabsVisible: false}
                                            PropertyChanges{target:skyObjView; flipped: true}
                                        }
                                    ]

                                    anchors{
                                        top: infoBoxTab.top
                                        bottom: infoBoxTab.bottom
                                        left: infoBoxTab.left
                                        right: infoBoxTab.right
                                        rightMargin: 4
                                        leftMargin: 4
                                    }
                                    border.color: "#585454"

                                    Flickable {
                                        id: flickableInfoText
                                        clip: true
                                        flickableDirection: Flickable.VerticalFlick
                                        width: parent.width
                                        height: parent.height - 10
                                        anchors{
                                            top: parent.top
                                            topMargin: 10
                                            bottom: parent.bottom
                                            bottomMargin: 4
                                            left: parent.left
                                            right: parent.right
                                        }
                                        contentWidth:   parent.width
                                        contentHeight: col2.height + 4
                                        Item {
                                            id: descInfoTextItem
                                            anchors{
                                                top: parent.top
                                                topMargin: 0
                                                left: parent.left
                                                leftMargin: 4
                                                right: parent.right
                                                rightMargin: 4
                                            }
                                            Column {
                                                id: col2
                                                width: parent.width
                                                Text {
                                                    id: infoText
                                                    objectName: "infoBoxText"
                                                    textFormat: Text.RichText
                                                    color: "white"
                                                    onLinkActivated: Qt.openUrlExternally(link)
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        acceptedButtons: Qt.NoButton // we don't want to eat clicks on the Text
                                                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                                                    }

                                                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                                    verticalAlignment: Text.AlignVCenter
                                                    horizontalAlignment: Text.AlignHCenter
                                                    text: xi18n("Info Text")
                                                    clip: true
                                                    width: parent.width
                                                    font{
                                                        family: "Arial"
                                                        pixelSize: 13
                                                    } //font
                                                } //text
                                            } //column
                                        } //item
                                    } //flickable
                                } //rectangle
                            } //tab
                        } //tabview



                        Item {
                            id: nextObjRect
                            objectName: "nextObj"
                            width: nextObjText.width + nextObjIcon.width + 10
                            height: 28
                            anchors {
                                right: parent.right
                                rightMargin: 10
                                bottom: parent.bottom
                                bottomMargin: 0
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
                                    nextObjText.color = (nightVision.state == "active") ? "red" : "yellow"
                                }
                                onExited: {
                                    nextObjForeground.opacity = 0.0
                                    nextObjText.color = (nightVision.state == "active") ? "red" : "white"
                                }
                                onClicked: {
                                    nextObjRect.nextObjClicked()
                                    soListView.positionViewAtIndex(soListView.currentIndex, ListView.Beginning)
                                }
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
                                bottomMargin: 0
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
                                    prevObjText.color = (nightVision.state == "active") ? "red" : "yellow"
                                }
                                onExited: {
                                    prevObjForeground.opacity = 0.0
                                    prevObjText.color = (nightVision.state == "active") ? "red" : "white"
                                }
                                onClicked: {
                                    prevObjRect.prevObjClicked()
                                    soListView.positionViewAtIndex(soListView.currentIndex, ListView.Beginning)
                                }
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


                    } //end of detailsView

                    Rectangle{
                        id: soItemEmptyMessage
                        objectName: "soItemEmptyMessage"
                        color: "#00060b"
                        anchors.fill: parent
                        Text{
                            anchors.fill: parent
                            text: xi18n("No Items to display")
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            color: "white"
                            font{
                                family: "Arial"
                                pointSize: 20
                            }
                        }
                        visible: (soListView.count > 0 || container.state == "singleItemSelected") ? false : true
                    }

                } //end of detailsViewContainer

                focus:true

                Keys.onPressed: {
                    if (event.key == Qt.Key_Left||event.key == Qt.Key_Up) {
                        prevObjRect.prevObjClicked();
                        event.accepted = true;
                        soListView.positionViewAtIndex(soListView.currentIndex, ListView.Beginning)
                    }
                    if (event.key == Qt.Key_Right||event.key == Qt.Key_Down) {
                        nextObjRect.nextObjClicked();
                        event.accepted = true;
                        soListView.positionViewAtIndex(soListView.currentIndex, ListView.Beginning)
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

                Rectangle{
                    id: loadingMessage
                    objectName: "loadingMessage"
                    color: "#00060b"
                    anchors.fill: parent
                    visible: false
                    Text{
                        anchors.fill: parent
                        text: xi18n("Loading...")
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        color: "white"
                        font{
                            family: "Arial"
                            pointSize: 30
                        }
                    }
                    states: [
                        State {
                            name: "loading"
                            PropertyChanges {target: loadingMessage; visible: true }
                            PropertyChanges {target: skyObjView; flipped:false }
                        }
                    ]
                }
            } //end of skyObjView
        } //end of viewsContainer
        Rectangle{
            id: helpMessage
            objectName: "helpMessage"
            color: "#00060b"
            anchors.fill: parent
            visible: false
            Text{
                id: helpText
                anchors.left: helpMessage.left
                anchors.right: helpMessage.right
                anchors.margins: 10
                text: xi18n("Explanation of the What's Interesting Panel")
                horizontalAlignment: Text.AlignHCenter
                color: "white"
                font{
                    family: "Arial"
                    pointSize: 15
                }
            }
            Text{
                id: helpExplainText
                anchors.margins: 10
                anchors.top: helpText.bottom
                anchors.left: helpMessage.left
                anchors.right: helpMessage.right
                text: xi18n("The What's Interesting Panel is intended to allow you to explore many different interesting objects in the night sky.  It includes objects visible to the naked eye as well as objects that require telescopes.  It is intended to appeal to both beginners and advanced astronomers.  If you click on a category or catalog, a list of objects will appear.  Clicking on an object in the list will bring up the details view where you can find out more information about the object.  If you have thumbnail images or wikipedia information for this object, these will be displayed as well.  If not, you can download them using the download icon.  If you make What's Interesting wider, the display will dynamically change to display the information more conveniently.  Please see the descriptions below for details on what the buttons at the bottom do.")
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                color: "white"
                font{
                    family: "Arial"
                    pointSize: 11
                }
            }
            Image {
                id: helpSettingsImage
                anchors.top: helpExplainText.bottom
                source: "settingsIcon.png"
                width: 28
                height: 28
            }
            Text{
                id: helpSettingsText
                anchors.top: helpExplainText.bottom
                anchors.left: helpSettingsImage.right
                anchors.right: helpMessage.right
                anchors.margins: 10
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                text: xi18n("This button will bring up the What's Interesting Settings. It will let you configure what is displayed in What's Interesting based upon which equipment you are using and the observing conditions.")
                color: "white"
                font{
                    family: "Arial"
                    pointSize: 11
                }
            }
            Image {
                id: helpInspectImage
                anchors.top: helpSettingsText.bottom
                source: "inspectIcon.png"
                width: 28
                height: 28
            }
            Text{
                id: helpInspectText
                anchors.top: helpSettingsText.bottom
                anchors.left: helpInspectImage.right
                anchors.right: helpMessage.right
                anchors.margins: 10
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                text: xi18n("This button will turn on and off the Inspector Mode.  In this mode you can click on any object in the map and What's Interesting will display the information about it.")
                color: "white"
                font{
                    family: "Arial"
                    pointSize: 11
                }
            }
            Image {
                id: helpReloadImage
                anchors.top: helpInspectText.bottom
                source: "reloadIcon.png"
                width: 28
                height: 28
            }
            Text{
                id: helpReloadText
                anchors.top: helpInspectText.bottom
                anchors.left: helpReloadImage.right
                anchors.right: helpMessage.right
                anchors.margins: 10
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                text: xi18n("This button will reload the current object list, update all displayed information, update any images, and update the information and images for the currently selected object.")
                color: "white"
                font{
                    family: "Arial"
                    pointSize: 11
                }
            }
            Image {
                id: helpVisibleImage
                anchors.top: helpReloadText.bottom
                source: "visibleIcon.png"
                width: 28
                height: 28
            }
            Text{
                id: helpVisibleText
                anchors.top: helpReloadText.bottom
                anchors.left: helpVisibleImage.right
                anchors.right: helpMessage.right
                anchors.margins: 10
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                text: xi18n("This button will toggle whether to filter the list to display only currently visible objects in a list or to display all of the objects in the list.  The visibility is determined based on the current KStars date and time, the current observing equipment, and the current sky conditions based on the What's Interesting Settings.")
                color: "white"
                font{
                    family: "Arial"
                    pointSize: 11
                }
            }
            Image {
                id: helpFavoriteImage
                anchors.top: helpVisibleText.bottom
                source: "favoriteIcon.png"
                width: 28
                height: 28
            }
            Text{
                id: helpFavoriteText
                anchors.top: helpVisibleText.bottom
                anchors.left: helpFavoriteImage.right
                anchors.right: helpMessage.right
                anchors.margins: 10
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                text: xi18n("This button will toggle whether to filter the list to display only 'interesting' objects or to display any of the objects in the list.  This setting only applies to the Galaxies, Nebulas, and Clusters lists.  The objects are considered 'interesting' if they appear on the KStars 'interesting' list.")
                color: "white"
                font{
                    family: "Arial"
                    pointSize: 11
                }
            }
            Image {
                id: helpDownloadImage
                anchors.top: helpFavoriteText.bottom
                source: "downloadIcon.png"
                width: 28
                height: 28
            }
            Text{
                id: helpDownloadText
                anchors.top: helpFavoriteText.bottom
                anchors.left: helpDownloadImage.right
                anchors.right: helpMessage.right
                anchors.margins: 10
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                text: xi18n("This button will attempt to download information and pictures about the object(s) from Wikipedia.  You can select whether to download the information about just one object, all of the objects in a list, or only the objects in a list for which no data was downloaded yet.  Please note: If the list is currently filtered for visible objects or 'interesting' objects, only the filtered objects will be downloaded.  If you actually want all the objects in the list, turn off the filters.")
                color: "white"
                font{
                    family: "Arial"
                    pointSize: 11
                }
            }
            states: [
                State {
                    name: "helpDisplayed"
                    PropertyChanges {target: helpMessage; visible: true }
                    PropertyChanges {target: backButton; x: container.width - 105}
                }
            ]
        }
    } //end of base

    Rectangle {
        id: backButton
        x: container.width + 10
        y: container.height - 50
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
                if(helpMessage.state == "helpDisplayed"){
                    helpMessage.state = ""
                } else if (container.state == "objectFromListSelected") {
                    if (!skyObjView.flipped||container.width>=900) {
                        container.state = "base"
                        catTitle.text = ""
                    } else if (skyObjView.flipped) {
                        skyObjView.flipped = false
                    }
                } else if (container.state == "singleItemSelected") {
                    container.state = "base"
                    catTitle.text = ""
                    if (container.width>=900) {
                        skyObjView.flipped = true
                    } else{
                        skyObjView.flipped = false
                    }
                }
            }
        }
    }

    Image {
        id: settingsIcon
        objectName: "settingsIconObj"
        x: 10
        y: container.height - 50
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
        id: inspectIcon
        objectName: "inspectIconObj"
        state: "checked"
        x: 50
        y: container.height - 50
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
        source: "inspectIcon.png"

        signal inspectIconClicked(bool inspect)

        MouseArea {
            id: inspectMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: inspectForeground.opacity = buttonOpacity
            onExited: inspectForeground.opacity = 0.0
            onClicked: {
                inspectIcon.inspectIconClicked(inspectIcon.state == "checked")
                inspectIcon.state = (inspectIcon.state == "checked") ?  "" : "checked"
            }
        }

        Rectangle {
            id: inspectForeground
            radius: 5
            opacity: 0
            anchors.fill: parent
        }
        states: [
            State {
                name: "checked"
                PropertyChanges {target: inspectIcon; opacity: 0.5}
            }
        ]
    }

    Image {
        id: reloadIcon
        objectName: "reloadIconObj"
        x: 90
        y: container.height - 50
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
            onClicked: {
                reloadIcon.reloadIconClicked();
            }
        }

        Rectangle {
            id: reloadForeground
            radius: 5
            opacity: 0
            anchors.fill: parent
        }
        states: [
            State {
                name: "invisible"
                when:  (container.state != "objectFromListSelected" && container.state != "singleItemSelected")
                PropertyChanges {target: reloadMouseArea; enabled: false}
                PropertyChanges {target: reloadIcon; opacity: 0}
            }
        ]

    }

    Image {
        id: visibleIcon
        objectName: "visibleIconObj"
        state: "checked"
        x: 130
        y: container.height - 50
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
        source: "visibleIcon.png"

        signal visibleIconClicked(bool visible)

        MouseArea {
            id: visibleMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: visibleForeground.opacity = buttonOpacity
            onExited: visibleForeground.opacity = 0.0
            onClicked: {
                visibleIcon.visibleIconClicked(visibleIcon.state == "unchecked")
                visibleIcon.state = (visibleIcon.state == "unchecked") ?  "" : "unchecked"
            }
        }

        Rectangle {
            id: visibleForeground
            radius: 5
            opacity: 0
            anchors.fill: parent
        }
        states: [
            State {
                name: "invisible"
                when: container.state != "objectFromListSelected"
                PropertyChanges {target: visibleMouseArea; enabled: false}
                PropertyChanges {target: visibleIcon; opacity: 0}
            },
            State {
                name: "unchecked"
                PropertyChanges {target: visibleIcon; opacity: 0.5}
            }
        ]
    }

    Image {
        id: favoriteIcon
        objectName: "favoriteIconObj"
        state: "checked"
        x: 170
        y: container.height - 50
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
        source: "favoriteIcon.png"

        signal favoriteIconClicked(bool favorite)

        MouseArea {
            id: favoriteMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: favoriteForeground.opacity = buttonOpacity
            onExited: favoriteForeground.opacity = 0.0
            onClicked: {
                favoriteIcon.favoriteIconClicked(favoriteIcon.state == "unchecked")
                favoriteIcon.state = (favoriteIcon.state == "unchecked") ?  "" : "unchecked"
            }
        }
        /**
           ToolTip{
           id: toolTip
           text: "Toggles the display of *interesting* objects vs. all objects.  \n(For Galaxies, Nebulas, and Clusters Only!)"
           }
        **/

        Rectangle {
            id: favoriteForeground
            radius: 5
            opacity: 0
            anchors.fill: parent
        }
        states: [
            State {
                name: "invisible"
                when:  container.state != "objectFromListSelected"
                PropertyChanges {target: favoriteMouseArea; enabled: false}
                PropertyChanges {target: favoriteIcon; opacity: 0}
            },
            State {
                name: "unchecked"
                PropertyChanges {target: favoriteIcon; opacity: 0.5}
            }
        ]
    }

    Image {
        id: downloadIcon
        objectName: "downloadIconObj"
        x: 210
        y: container.height - 50
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
        source: "downloadIcon.png"

        signal downloadIconClicked

        MouseArea {
            id: downloadMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: downloadForeground.opacity = buttonOpacity
            onExited: downloadForeground.opacity = 0.0
            onClicked: downloadIcon.downloadIconClicked()
        }

        Rectangle {
            id: downloadForeground
            radius: 5
            opacity: 0
            anchors.fill: parent
        }
        states: [
            State {
                name: "invisible"
                when: container.state == "base" || container.state == ""
                PropertyChanges {target: downloadMouseArea; enabled: false}
                PropertyChanges {target: downloadIcon; opacity: 0}
            }
        ]
    }

    Image {
        id: helpIcon
        x: 250
        y: container.height - 50
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
        source: "helpIcon.png"

        MouseArea {
            id: helpMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: helpForeground.opacity = buttonOpacity
            onExited: helpForeground.opacity = 0.0
            onClicked: (helpMessage.state == "helpDisplayed") ? helpMessage.state = "" : helpMessage.state = "helpDisplayed"
        }

        Rectangle {
            id: helpForeground
            radius: 5
            opacity: 0
            anchors.fill: parent
        }
    }

    Rectangle {
        id: nightVision
        objectName: "nightVision"
        opacity: 0
        color: "#510000"
        anchors.fill: parent

        states: [
            State {
                name: "active"
                PropertyChanges {target: nightVision; opacity: 0.2}
                PropertyChanges {target: tabbedView; style: tabbedView.nightTabs}
                PropertyChanges {target: title; color: "red"}
                PropertyChanges {target: catTitle; color: "red"}
                PropertyChanges {target: nakedEyeText; color: "red"}
                PropertyChanges {target: dsoText; color: "red"}
                PropertyChanges {target: catalogText; color: "red"}
                PropertyChanges {target: soListEmptyMessage; color: "red"}
                PropertyChanges {target: soItemEmptyMessage; color: "red"}
                PropertyChanges {target: scrollbar; color: "red"}
                PropertyChanges {target: prevObjText; color: "red"}
                PropertyChanges {target: nextObjText; color: "red"}
                PropertyChanges {target: detailsText; color: "red"}
                PropertyChanges {target: soname; color: "red"}
                PropertyChanges {target: posText; color: "red"}
                PropertyChanges {target: detailsButton; color: "red"}
                PropertyChanges {target: centerButton; color: "red"}
                PropertyChanges {target: slewTelescopeButton; color: "red"}
                PropertyChanges {target: goBackText; color: "red"}
            }
        ]

    }

    states: [
        State {
            name: "base"

        },
        State {
            name: "singleItemSelected"

            PropertyChanges {
                target: viewsRow
                x: -(2 * categoryView.width)
                y: 0
                anchors{
                    topMargin: 0
                    bottomMargin: 0
                }
            }

            PropertyChanges{target:skyObjView; flipped: true}

            PropertyChanges {
                target: backButton
                x: container.width - 105
            }
        },
        State {
            name: "objectFromListSelected"

            PropertyChanges {
                target: viewsRow
                x: -(2 * categoryView.width)
                y: 0
                anchors{
                    topMargin: 0
                    bottomMargin: 0
                }
            }
            //PropertyChanges {target: detailsView; focus: true}
            PropertyChanges {
                target: backButton
                x: container.width - 105
            }
        }
    ]

    transitions: [

        Transition {
            from: "*"
            to: "objectFromListSelected"
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
            from: "objectFromListSelected"
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
        }
    ]
}
