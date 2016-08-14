import QtQuick 2.7

import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0

import QtQuick.Window 2.2 as Window
import QtQuick.Layouts 1.1
import "modules"
import "modules/helpers"
import "modules/popups"
import "dialogs"
import "constants" 1.0
import "indi"

ApplicationWindow {
    id: window
    objectName: "window"
    width: Window.Screen.desktopAvailableWidth
    height: Window.Screen.desktopAvailableHeight
    visible: true
    property bool isPortrait: width < height ? true: false
    property bool isSkyMapVisible: stackView.currentItem == initPage

    //Application properties
    property bool loaded: false

    header: ToolBar {
        id: toolBar
        Material.foreground: "white"
        height: stackView.currentItem != initPage ? backButton.height: 0
        visible: stackView.currentItem != initPage

        Behavior on height {
            NumberAnimation {
                duration: 200
                easing.type: Easing.InOutQuad
            }
        }

        RowLayout {
            spacing: 20
            height: titleLabel.height
            Layout.fillWidth: true

            ToolButton {
                id: backButton
                contentItem: Image {
                    fillMode: Image.Pad
                    horizontalAlignment: Image.AlignHCenter
                    verticalAlignment: Image.AlignVCenter
                    source: "modules/images/back.png"
                    sourceSize.height: titleLabel.height
                }
                onClicked: {
                    if(stackView.depth != 1) stackView.pop()
                }
            }

            Label {
                id: titleLabel
                text: stackView.currentItem.title
                font.pixelSize: 20
                elide: Label.ElideRight
                //horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }

            ToolButton {
                /*contentItem: Image {
                    fillMode: Image.Pad
                    horizontalAlignment: Image.AlignHCenter
                    verticalAlignment: Image.AlignVCenter
                    //source: "qrc:/images/menu.png"
                }*/
                onClicked: optionsMenu.open()

                Menu {
                    id: optionsMenu
                    x: parent.width - width
                    transformOrigin: Menu.TopRight

                    MenuItem {
                        text: "Settings"
                        onTriggered: settingsPopup.open()
                    }
                    MenuItem {
                        text: "About"
                        onTriggered: aboutDialog.open()
                    }
                }
            }
        }
    }

    Splash {
        z:1
        anchors.fill:parent
        onTimeout: {
            loaded = true
        }
    }

    StackView {
        visible: loaded
        id: stackView
        anchors.fill: parent
        initialItem: initPage
    }

    PassiveNotification {
        id: notification
    }

    Units {
        id: units
    }

    //Pages
    FindDialog {
        id: findDialog
    }

    INDIControlPanel {
        id: indiControlPanel
    }

    ObjectDetails {
        id: objectDetails
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

    ObjectPopup {
        id: objPopup
        x: (window.width - width)/2
        y: (window.height - height)/2
    }

    Drawer {
        id: globalDrawer
        width: Math.min(window.width, window.height) / 4 * 2
        height: window.height
        //Disable drawer while loading
        dragMargin: loaded ? Qt.styleHints.startDragDistance : -Qt.styleHints.startDragDistance

        onOpened: {
            contextDrawer.close()
        }

        Image {
            id: drawerBanner
            source: "modules/images/kstars.png"
            fillMode: Image.PreserveAspectFit

            anchors {
                left: parent.left
                top: parent.top
                right: parent.right
            }
        }

        ListView {
            id: pagesList
            currentIndex: -1
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
                highlighted: ListView.isCurrentItem
                onClicked: {
                    if (pagesList.currentIndex != index) {
                        pagesList.currentIndex = index

                        if(stackView.currentItem != model.objID) {
                            if(model.objID != initPage) {
                                //skyMapLiteWrapper.visible = false
                                stackView.replace(null, [initPage, model.objID])
                            } else {
                                stackView.replace(null, initPage)
                                //skyMapLiteWrapper.visible = true
                            }
                        }
                        globalDrawer.close()
                        pagesList.currentIndex = -1
                    }
                }
            }

            property ListModel drawerModel : ListModel {
                //Trick to enable storing of object ids
                Component.onCompleted: {
                    append({objID: initPage});
                    append({objID: indiControlPanel});
                    append({objID: findDialog});
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
        dragMargin: isSkyMapVisible && loaded ? Qt.styleHints.startDragDistance : -Qt.styleHints.startDragDistance
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
            currentIndex: -1
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
                highlighted: ListView.isCurrentItem
                onClicked: {
                    if(model.type == "popup") {
                        objID.open()
                    }
                    contextDrawer.close()
                    contextList.currentIndex = -1
                }
            }

            property ListModel drawerModel : ListModel {
                //Trick to enable storing of object ids
                Component.onCompleted: {
                    append({title: "Projection systems", objID: projPopup, type: "popup"});
                    append({title: "Color Schemes", objID: colorSchemePopup, type: "popup"});
                }
            }

            ScrollIndicator.vertical: ScrollIndicator { }
        }
        edge: Qt.RightEdge
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
