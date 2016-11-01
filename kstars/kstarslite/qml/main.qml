// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

import QtQuick 2.7

import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0

import QtQuick.Window 2.2 as Window
import QtQuick.Layouts 1.1

import "modules"
import "modules/helpers"
import "modules/popups"
import "modules/menus"
import "modules/tutorial"


import "dialogs"
import "dialogs/menus"
import "dialogs/helpers"

import "constants" 1.0
import "indi"

ApplicationWindow {
    id: window
    objectName: "window"
    width: Window.Screen.desktopAvailableWidth
    height: Window.Screen.desktopAvailableHeight
    visible: true

    //Application properties
    property bool isLoaded: false
    property bool isPortrait: width < height ? true: false
    property bool isSkyMapVisible: stackView.currentItem == initPage
    signal loaded();

    onIsLoadedChanged: {
        if(isLoaded) {
            loaded()
            if(KStarsLite.runTutorial) tutorialPopup.open()
        }
    }

    header: ToolBar {
        id: toolBar
        Material.foreground: "white"
        height: stackView.currentItem != initPage ? backButton.height : 0
        visible: stackView.currentItem != initPage

        background: Rectangle {
            anchors.fill: parent
            color: num.sysPalette.dark
        }

        Behavior on height {
            NumberAnimation {
                duration: 200
                easing.type: Easing.InOutQuad
            }
        }

        Row {
            id: toolRow
            spacing: 20
            height: parent.height
            width: parent.width

            ToolButton {
                id: backButton
                contentItem: Image {
                    fillMode: Image.Pad
                    horizontalAlignment: Image.AlignHCenter
                    verticalAlignment: Image.AlignVCenter
                    source: "images/back.png"
                }
                onClicked: {
                    if(stackView.depth != 1) stackView.pop()
                }
            }

            KSLabel {
                id: titleLabel
                text: stackView.currentItem.title

                font.pixelSize: 20
                width: parent.width - backButton.width - toolRow.spacing //To allow ellision of the text

                elide: Label.ElideRight
                wrapMode: Label.Wrap
                maximumLineCount: 1

                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Splash {
        z:1
        anchors.fill:parent
        onTimeout: {
            isLoaded = true
        }
    }

    StackView {
        visible: isLoaded
        id: stackView
        anchors.fill: parent
        initialItem: initPage
    }

    PassiveNotification {
        z: 2
        id: notification

        Connections {
            target: KStarsLite
            onNotificationMessage: {
                notification.showNotification(msg)
            }
        }
    }

    Units {
        id: units
    }

    //Dialogs
    FindDialog {
        id: findDialog
    }

    //Details
    DetailsDialog {
        id: detailsDialog
    }

    DetailsAddLink {
        id: detailsAddLink
    }

    DetailsLinkMenu {
        id: detailsLinkMenu
        x: (window.width - width)/2
        y: (window.height - height)/2
    }

    //Location
    LocationDialog {
        id: locationDialog
    }

    LocationEdit {
        id: locationEdit
    }

    LocationLoading {
        id: locationLoading
    }

    //Pages
    INDIControlPanel {
        id: indiControlPanel
    }

    Page {
        id: initPage
        title: "Sky Map"
        padding: 0

        Rectangle {
            anchors.fill: parent
            color: "black" //Color scheme
        }
    }

    SkyMapLiteWrapper {
        /*The reason SkyMapLite is a not a child of initPage is that it can't handle properly change of
          opacity. Each time we go from / to initPage this component is made invisible / visible and
        skyMapLiteWrapper is anchored to fill null / parent*/
        id: skyMapLite
        anchors.fill: parent
    }

    //Popups
    TimePage {
        id: timePage
    }

    ColorSchemePopup {
        id: colorSchemePopup
        x: (window.width - width)/2
        y: (window.height - height)/2
    }

    ProjectionsPopup {
        id: projPopup
        x: (window.width - width)/2
        y: (window.height - height)/2
    }

    FOVPopup {
        id: fovPopup
        x: (window.width - width)/2
        y: (window.height - height)/2
    }

    TutorialPopup {
        id: tutorialPopup
        x: (window.width - width)/2
        y: (window.height - height)/2
    }

    TutorialExitPopup {
        id: tutorialExitPopup
        x: (window.width - width)/2
        y: (window.height - height)/2
    }

    //Menus
    ContextMenu {
        id: contextMenu
        x: (window.width - width)/2
        y: (window.height - height)/2
    }

    LocationsGeoMenu {
        id: locationsGeoMenu
        x: (window.width - width)/2
        y: (window.height - height)/2
    }

    Drawer {
        id: globalDrawer
        width: Math.min(window.width, window.height) / 4 * 2
        height: window.height
        //Disable drawer while loading
        dragMargin: isLoaded ? Qt.styleHints.startDragDistance : -Qt.styleHints.startDragDistance
        background: Rectangle {
            anchors.fill: parent
            color: num.sysPalette.base
        }

        onOpened: {
            contextDrawer.close()
        }

        Image {
            id: drawerBanner
            source: "images/kstars.png"
            fillMode: Image.PreserveAspectFit

            anchors {
                left: parent.left
                top: parent.top
                right: parent.right
            }
        }

        ListView {
            clip: true
            id: pagesList
            anchors {
                left: parent.left
                top: drawerBanner.bottom
                right: parent.right
                bottom: parent.bottom
            }

            delegate: ItemDelegate {
                id: globalDrawerControl
                Rectangle {
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                    }
                    width: parent.width - 10
                    color: "#E8E8E8"
                    height: 1
                }

                contentItem: KSText {
                    rightPadding: globalDrawerControl.spacing
                         text: globalDrawerControl.text
                         font: globalDrawerControl.font
                         elide: Text.ElideRight
                         visible: globalDrawerControl.text
                         horizontalAlignment: Text.AlignLeft
                         verticalAlignment: Text.AlignVCenter
                }

                width: parent.width
                text: model.objID.title
                onClicked: {
                    if(stackView.currentItem != model.objID) {
                        if(model.objID != initPage) {
                            stackView.replace(null, [initPage, model.objID])
                        } else {
                            stackView.replace(null, initPage)
                        }
                        globalDrawer.close()
                    }
                }
            }

            property ListModel drawerModel : ListModel {
                //Trick to enable storing of object ids
                Component.onCompleted: {
                    append({objID: initPage});
                    append({objID: indiControlPanel});
                    append({objID: findDialog});
                    append({objID: locationDialog});
                }
            }

            model: drawerModel

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }

    //Study mode
    property bool step1: false
    property bool step2: false
    property bool step3: false
    property bool step4: false
    property bool step5: false

    function askExitTutorial() {
        tutorialExitPopup.open()
    }

    function exitTutorial() {
        KStarsLite.runTutorial = false
        tutorialPopup.close()
        step1 = false
        step2 = false
        step3 = false
        step4 = false
        step5 = false
    }

    //Step 1 - Global Drawer
    TutorialStep1 {

    }

    //Step 2 - Context Drawer
    TutorialStep2 {

    }

    //Step 5 - Location
    TutorialStep5 {

    }

    Drawer {
        id: contextDrawer
        width: Math.min(window.width, window.height) / 4 * 2
        height: window.height
        //Disable drawer while loading and if SkyMapLite is not visible
        dragMargin: isSkyMapVisible && isLoaded ? Qt.styleHints.startDragDistance + 15 : -Qt.styleHints.startDragDistance
        edge: Qt.RightEdge
        background: Rectangle {
            anchors.fill: parent
            color: num.sysPalette.base
        }

        onOpened: {
            globalDrawer.close()
        }

        KSLabel {
            id: contextTitle
            anchors {
                top: parent.top
                left: parent.left
                margins: 10
            }

            font.pointSize: 14
            text: stackView.currentItem.title
        }

        ListView {
            id: contextList
            anchors {
                left: parent.left
                top: contextTitle.bottom
                right: parent.right
                bottom: parent.bottom
                topMargin: 15
            }
            model: drawerModel

            delegate: ItemDelegate {
                id: contextDrawerControl
                Rectangle {
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                    }
                    width: parent.width - 10
                    color: "#E8E8E8"
                    height: 1
                }

                width: parent.width
                text: model.title
                onClicked: {
                    if(model.type == "popup") {
                        objID.open()
                    }
                    contextDrawer.close()
                }

                contentItem: KSText {
                    rightPadding: contextDrawerControl.spacing
                         text: contextDrawerControl.text
                         font: contextDrawerControl.font
                         elide: Text.ElideRight
                         visible: contextDrawerControl.text
                         horizontalAlignment: Text.AlignLeft
                         verticalAlignment: Text.AlignVCenter
                }
            }

            property ListModel drawerModel : ListModel {
                //Trick to enable storing of object ids
                Component.onCompleted: {
                    append({title: xi18n("Projection systems"), objID: projPopup, type: "popup"});
                    append({title: xi18n("Color Schemes"), objID: colorSchemePopup, type: "popup"});
                    append({title: xi18n("FOV Symbols"), objID: fovPopup, type: "popup"});
                }
            }

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        ColumnLayout {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }

            RowLayout {
                id: magnitudeRow
                anchors {
                    leftMargin: 10
                    left: parent.left
                    rightMargin: 10
                    right: parent.right
                }

                property color magnitudeColor: colorSchemePopup.currentCScheme == "cs_night" ? "white" : "black"

                Rectangle {
                    anchors{
                        left: parent.left
                        bottom: smallestMag.bottom
                    }

                    width: 24
                    height: 24
                    radius: width * 0.5
                    color: magnitudeRow.magnitudeColor
                }

                Rectangle {
                    anchors{
                        horizontalCenter: parent.horizontalCenter
                        bottom: smallestMag.bottom
                    }

                    width: 16
                    height: 16
                    radius: width * 0.5
                    color: magnitudeRow.magnitudeColor
                }

                Rectangle {
                    id: smallestMag

                    anchors {
                        right: parent.right
                        verticalCenter: parent.bottom
                    }

                    width: 8
                    height: 8
                    radius: width * 0.5
                    color: magnitudeRow.magnitudeColor
                }
            }

            Slider {
                id: magSlider
                anchors {
                    left: parent.left
                    right: parent.right
                }

                from: 1.18778
                to: 5.75954
                value: SkyMapLite.magLim

                onValueChanged: {
                    SkyMapLite.magLim = value
                }
            }
        }
    }

    //Handle back button
    Connections {
        target: window
        onClosing: {
            if (Qt.platform.os == "android") {
                if(stackView.depth > 1) {
                    close.accepted = false;
                    stackView.pop()
                }
            }
        }
    }
}
