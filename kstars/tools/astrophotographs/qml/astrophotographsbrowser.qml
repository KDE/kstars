// Copyright (C) 2014 Vijay Dhameliya <vijay.atwork13@gmail.com>
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

import QtQuick 1.1
import "../../whatsinteresting/qml/"

Rectangle {
    id: container
    objectName: "containerObj"
    width: 375
    height: 575
    color: "#020518"
    anchors.fill: parent

    Text {
        id: title
        width: 210
        height: 45
        color: "#59ad0e"
        text: i18n("Astrophotographs Browser")
        verticalAlignment: Text.AlignVCenter
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.left: parent.left
        anchors.leftMargin: 20
        font.family: "Cantarell"
        font.bold: false
        font.pixelSize: 22
    }

    Rectangle {
        id: searchContainer
        objectName: "searchContainerObj"
        y: 90
        width: parent.width - 100
        height: 40
        color: "transparent"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20

        signal searchButtonClicked()

        Rectangle {
            id: searchTextContainer
            width: parent.width - searchButton.vwidth - 10
            height: parent.height
            color: "#00060b"
            radius: 10
            opacity: 0.500
            border.width: 4
            border.color: "black"

            Rectangle {
                width: parent.width
                height: parent.height
                color: "transparent"
                anchors.top: parent.top
                anchors.topMargin: 10
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 10
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 10

                TextInput {
                    id: searchBar
                    objectName: "searchBarObj"
                    width: parent.width
                    height: parent.height
                    color: "#59ad0e"
                    text: "Quick Search..."
                    font.pixelSize: 18
                }
            }

        }

    }

    Rectangle {
        anchors.left: searchContainer.right
        anchors.leftMargin: -60
        anchors.top: parent.top
        anchors.topMargin: 90
        width: parent.width - 100
        height: 40
        color: "transparent"

        PushButton {
            id: searchButton
            vheight: parent.height - 4
            text: "Search"
        }

        MouseArea{
            anchors.fill: parent
            onClicked: {
                searchContainer.searchButtonClicked()
            }
        }
    }

    Rectangle {
        id: dataContainer
        width: parent.width
        height: parent.height - title.height - searchContainer.height - 120
        color: "#00060b"
        radius: 10
        opacity: 0.9
        border.width: 4
        border.color: "black"
        anchors.top: searchContainer.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20

        Flipable {
            id: resultView
            anchors.fill: parent

            property bool flipped: false

            front: Rectangle {
                id: astrophotoListContainer
                anchors.fill: parent
                anchors.margins: 5
                color: "transparent"

                ListModel {
                    id: resultModel

                    // this elements are to be added dynamically
                    ListElement {
                        path: "/home/vijay13/Pictures/1.jpg"
                        name: "Sky Object One"
                        date: "14th April 2014"
                    }

                    ListElement {
                        path: "/home/vijay13/Pictures/2.jpg"
                        name: "Sky Object Two"
                        date: "15th April 2014"
                    }

                    ListElement {
                        path: "/home/vijay13/Pictures/3.jpg"
                        name: "Sky Object Three"
                        date: "16th April 2014"
                    }

                    ListElement {
                        path: "/home/vijay13/Pictures/4.jpg"
                        name: "Sky Object Four"
                        date: "16th April 2014"
                    }

                    ListElement {
                        path: "/home/vijay13/Pictures/2.jpg"
                        name: "Sky Object Five"
                        date: "16th April 2014"
                    }

                }

                Component {
                    id: resultDelegate
                    Row {
                        spacing: 10
                        AstrophotographItem {
                            id: testItem
                            imagePath: path
                            imageName: name
                            imageDate: date
                            width: astrophotoListContainer.width - 20
                            height: 100

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    resultView.flipped = true
                                }
                            }
                        }
                    }
                }

                ListView {
                    id: resultListView
                    model: resultModel
                    delegate: resultDelegate
                    anchors.fill: parent
                    clip: true

                    ScrollBar {
                        flickable: resultListView
                    }
                }

            }

            back: Rectangle {
                id: detailViewContainer
                opacity: 1.0
                width: parent.width - 20
                height: parent.height - 20
                color: "transparent"
                anchors.left: parent.left
                anchors.leftMargin: 50
                anchors.right: parent.right
                anchors.rightMargin: -30
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    id: titleOfAstroPhotograph
                    width: detailViewContainer.width
                    text: qsTr("This is title of Astrophotograph downloaded from Astrobin")
                    color: "#ffffff"
                    anchors.top: detailViewContainer.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    horizontalAlignment: Text.AlignHCenter
                    font.bold: true
                    font.italic: true
                    font.pixelSize: 16
                }

                Rectangle {
                    id: photoContainer
                    width: detailViewContainer.width
                    height: ( detailViewContainer.height - titleOfAstroPhotograph.height ) / 1.85
                    anchors.top: titleOfAstroPhotograph.bottom
                    anchors.topMargin: 5
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "transparent"

                    Image {
                        id: astroPhotographImage
                        anchors.fill: parent
                        smooth: true
                        source: "/home/vijay13/Pictures/1.jpg"
                    }

                }

                Rectangle {
                    id: detailOfAstroPhotograph
                    color: "#010a14"
                    radius: 10
                    border.width: 0
                    anchors.top: photoContainer.bottom
                    anchors.topMargin: 5
                    anchors.right: parent.right
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    border.color: "#585454"

                    ListModel {
                        id: detailModel

                        // this elements are to be added dynamically
                        ListElement {
                            detailItemText: "Date: "
                        }

                    }

                    Component {
                        id: detailDelegate

                        Rectangle {
                            anchors.topMargin: 10
                            width: detailOfAstroPhotograph.width
                            height: detailItem.height + 5
                            color: "transparent"

                            Text {
                                id: detailItem
                                horizontalAlignment: Text.AlignLeft
                                color: "white"
                                text: detailItemText
                            }

                        }

                    }

                    ListView {
                        id: detailList
                        model: detailModel
                        delegate: detailDelegate
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.topMargin: 10
                        clip: true

                        ScrollBar {
                            flickable: detailList
                        }
                    }
                }

            }

            states: [
                State {
                    name: "back"
                    PropertyChanges {
                        target: listToDetailsRotation
                        angle: 180
                    }

                    PropertyChanges {
                        target: homeButtonContainer
                        opacity: 0
                    }

                    PropertyChanges {
                        target: buttonContainer
                        opacity: 1
                    }

                    when: resultView.flipped
                },
                State {
                    name: "front"
                    PropertyChanges {
                        target: listToDetailsRotation
                        angle: 0
                    }

                    PropertyChanges {
                        target: homeButtonContainer
                        opacity: 1
                    }

                    PropertyChanges {
                        target: buttonContainer
                        opacity: 0
                    }

                    when: !resultView.flipped
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

        }

    }

    Rectangle {
        id: footerContainer
        height: parent.height - title.height - searchContainer.height - dataContainer.height
        anchors.top: dataContainer.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        color: "transparent"

        Rectangle {
            id: homeButtonContainer
            width: 85
            height: footerContainer.height
            anchors.right: parent.right
            anchors.rightMargin: -5
            opacity: 1
            color: "transparent"

            Rectangle {
                id: homeButtonRect
                width: homeButtonContainer.width
                height: homeButtonContainer.height - 5
                anchors.right: homeButtonContainer.right
                anchors.verticalCenter: parent.verticalCenter
                color: "transparent"

                IconPushButton {
                    id: homeButton
                    iconPath: "/home/vijay13/kde/kstars/kstars/kstars/tools/whatsinteresting/qml/leftArrow.png"
                    text: "Home"
                    vheight: homeButtonRect.height
                    vwidth: homeButtonRect.width - 5
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        resultView.flipped = true
                    }
                }

            }
        }

        Rectangle {
            id: buttonContainer
            width: 240
            height: footerContainer.height
            anchors.right: parent.right
            anchors.rightMargin: -5
            opacity: 0
            color: "transparent"

            Rectangle {
                id: backButtonContainer
                width: buttonContainer.width / 3
                height: buttonContainer.height - 5
                anchors.left: buttonContainer.left
                anchors.verticalCenter: parent.verticalCenter
                color: "transparent"

                IconPushButton {
                    id: backButton
                    iconPath: "/home/vijay13/kde/kstars/kstars/kstars/tools/whatsinteresting/qml/leftArrow.png"
                    text: "Back"
                    vheight: backButtonContainer.height
                    vwidth: backButtonContainer.width - 5
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        resultView.flipped = false
                    }
                }

            }

            Rectangle {
                id: editButtonContainer
                width: buttonContainer.width / 3
                height: buttonContainer.height - 5
                anchors.left: backButtonContainer.right
                anchors.verticalCenter: parent.verticalCenter
                color: "transparent"

                IconPushButton {
                    id: editButton
                    iconPath: "/home/vijay13/kde/kstars/kstars/kstars/tools/whatsinteresting/qml/leftArrow.png"
                    text: "Edit"
                    vheight: editButtonContainer.height
                    vwidth: editButtonContainer.width - 5
                }
            }

            Rectangle {
                id: saveButtonContainer
                width: buttonContainer.width / 3
                height: buttonContainer.height - 5
                anchors.left: editButtonContainer.right
                anchors.verticalCenter: parent.verticalCenter
                color: "transparent"

                IconPushButton {
                    id: saveButton
                    iconPath: "/home/vijay13/kde/kstars/kstars/kstars/tools/whatsinteresting/qml/leftArrow.png"
                    text: "Save"
                    vheight: saveButtonContainer.height
                    vwidth: saveButtonContainer.width - 5
                }

            }
        }


    }
}
