import QtQuick 2.7

import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0

import QtQuick.Window 2.2 as Window
import QtQuick.Layouts 1.1

import "modules"
import "modules/helpers"
import "modules/popups"
import "modules/menus"

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
        if(isLoaded) loaded()
    }

    header: ToolBar {
        id: toolBar
        Material.foreground: "white"
        height: stackView.currentItem != initPage ? backButton.height : 0
        visible: stackView.currentItem != initPage

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

            Label {
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
        z: 10
        id: notification
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
            id: pagesList
            anchors {
                left: parent.left
                top: drawerBanner.bottom
                right: parent.right
                bottom: parent.bottom
            }

            delegate: ItemDelegate {
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
                text: model.objID.title
                onClicked: {
                    if(stackView.currentItem != model.objID) {
                        if(model.objID != initPage) {
                            //skyMapLiteWrapper.visible = false
                            stackView.replace(null, [initPage, model.objID])
                        } else {
                            stackView.replace(null, initPage)
                            //skyMapLiteWrapper.visible = true
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

    Drawer {
        id: contextDrawer
        width: Math.min(window.width, window.height) / 4 * 2
        height: window.height
        //Disable drawer while loading and if SkyMapLite is not visible
        dragMargin: isSkyMapVisible && isLoaded ? Qt.styleHints.startDragDistance + 15 : -Qt.styleHints.startDragDistance
        edge: Qt.RightEdge

        onOpened: {
            globalDrawer.close()
        }

        Label {
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
                id: row
                anchors {
                    leftMargin: 10
                    left: parent.left
                    rightMargin: 10
                    right: parent.right
                }

                Rectangle {
                    anchors{
                        left: parent.left
                        bottom: smallestMag.bottom
                    }

                    width: 24
                    height: 24
                    radius: width * 0.5
                    color: "black"
                }

                Rectangle {
                    anchors{
                        horizontalCenter: parent.horizontalCenter
                        bottom: smallestMag.bottom
                    }

                    width: 16
                    height: 16
                    radius: width * 0.5
                    color: "black"
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
                    color: "black"
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
