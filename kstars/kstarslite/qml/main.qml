import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import "modules"
import "indi"
import "constants" 1.0
import QtQuick.Controls 1.4 as Controls
import org.kde.kirigami 1.0 as Kirigami

Kirigami.ApplicationWindow {
    id: mainWindow
    width: Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight
    property double shadowBgOpacity
    property int drawersOrder: 2
    property int bgOrder: 1
    property int skyMapOrder: 0
    property int topMenuOrder: 0
    objectName: "mainWindow"

    property Item initPage: initPage

    pageStack.initialPage: initPage

    property var contextActions: []

    property var telescopes: []

    //pageStack.currentIndex: initPage

    property Item currentPage: initPage

    controlsVisible: false

    header: Kirigami.ApplicationHeader {
        visible: false
    }

    property bool loaded: false

    contentItem.anchors.topMargin: 0
    wideScreen: true


    Item {
        id: pagesWindow
        anchors.fill: parent
    }

    globalDrawer: Kirigami.GlobalDrawer {
        bannerImageSource: "modules/images/kstars.png"
        visible: loaded
        actions: [
            Kirigami.Action {
                text: "Sky Map"

                onTriggered: {
                    mainWindow.currentPage = initPage
                    initPage.showPage(true)
                    globalDrawer.close()
                }
            },
            Kirigami.Action {
                text: "INDI Control Panel"

                onTriggered: {
                    //mainWindow.currentPage = initPage
                    indiControlPanel.showPage(true)
                    globalDrawer.close()
                }
            }
        ]
    }

    contextDrawer: Kirigami.ContextDrawer {
        id: contextDrawer
        property Item contextMenu

        Component.onCompleted: {
            var contDrawMenuComp = Qt.createComponent("modules/ContextDrawerMenu.qml");
            contextMenu = contDrawMenuComp.createObject(contentItem)
        }
    }

    INDIControlPanel {
        id: indiControlPanel
    }

    KSPage {
        id: initPage
        title: "Main Screen"
        visible:true

        onVisibleChanged: {
            if(visible) {
                SkyMapLite.update() //Update SkyMapLite once user opened initPage
            }
        }

        property bool isClickedObject: false

        Connections {
            target: SkyMapLite
            onObjectChanged: {
                contextDrawer.title = ClickedObject.translatedName
                contextDrawer.open()
                initPage.isClickedObject = true
            }
            onPositionChanged: {
                contextDrawer.title = "Point"
                contextDrawer.open()
                initPage.isClickedObject = false
            }
        }

        property bool telescopeLoaded: false

        Connections {
            target: ClientManagerLite
            onTelescopeAdded: {
                telescopes.push(newTelescope)
                initPage.telescopeLoaded = true
            }
        }

        actions {
            contextualActions: [
                Kirigami.Action {
                    enabled: telescopeLoaded
                    text: "Slew to object"
                    onTriggered: {
                        var isConnected = false
                        for(var i = 0; i < telescopes.length; ++i) {
                            if(telescopes[i].isConnected()) {
                                isConnected = true
                                if(initPage.isClickedObject) {
                                    telescopes[i].slew(ClickedObject)
                                } else {
                                    telescopes[i].slew(ClickedPoint)
                                }
                            }
                        }
                        if(!isConnected) telescopeLoaded = false
                        else telescopeLoaded = true
                    }
                },
                Kirigami.Action {
                    enabled: telescopeLoaded
                    text: "Sync"
                    onTriggered: {
                        var isConnected = false
                        for(var i = 0; i < telescopes.length; ++i) {
                            if(telescopes[i].isConnected()) {
                                if(initPage.isClickedObject) {
                                    telescopes[i].sync(ClickedObject)
                                } else {
                                    telescopes[i].sync(ClickedPoint)
                                }
                            }
                        }
                        if(!isConnected) telescopeLoaded = false
                        else telescopeLoaded = true
                    }
                }
            ]
        }

        Splash {
            z:1
            anchors.fill:parent
            onTimeout: {
                loaded = true
                mainWindow.controlsVisible = true
            }
        }

        /*content is made Rectangle to allow z-index ordering
    (for some reason it doesn't work with plain Item)*/
        Item {
            id: content
            anchors.fill: parent
            visible: loaded

            Rectangle {
                id: skyMapLiteWrapper
                clip: true
                objectName: "skyMapLiteWrapper"
                anchors.fill: parent
                color: "black"

                onWidthChanged: {
                    SkyMapLite.width = width
                }
                onHeightChanged: {
                    SkyMapLite.height = height
                }
            }

            MouseArea {
                property int posY
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                }
                height: parent.height * 0.10

                onPressed: {
                    posY = mouseY
                }

                onPositionChanged: {
                    var ratio = 0.05
                    var delta = mouseY - posY
                    if (delta > parent.height * ratio) {
                        if(topMenu.state != "open") topMenu.state = "open"
                    } else if (delta < - (parent.height * ratio)) {
                        if(topMenu.state != "") topMenu.state = ""
                    }

                }
                TopMenu {
                    z: topMenuOrder
                    id: topMenu
                    anchors.horizontalCenter: parent.horizontalCenter
                    Connections {
                        target: mainWindow

                        function setOrientation() {
                            if (width < topMenu.width) {
                                topMenu.state = "portrait"
                            }
                            else topMenu.state = "landscape"
                        }

                        Component.onCompleted: {
                            setOrientation()
                        }

                        onWidthChanged: {
                            setOrientation()
                        }
                    }
                }
            }

            MouseArea {
                property int posY
                anchors {
                    bottom: parent.bottom
                    horizontalCenter: parent.horizontalCenter
                }
                width: bottomMenu.width
                height: parent.height *0.10

                onPressed: {
                    posY = mouseY
                }

                onPositionChanged: {
                    var ratio = 0.05
                    var delta = mouseY - posY
                    if (delta < parent.height * ratio) {
                        if(bottomMenu.state != "open") bottomMenu.state = "open"
                    } else if (delta > - (parent.height * ratio)) {
                        if(bottomMenu.state != "") bottomMenu.state = ""
                    }

                }
                BottomMenu {
                    z: topMenuOrder
                    id: bottomMenu

                    anchors.horizontalCenter: parent.horizontalCenter
                    Connections {
                        target: mainWindow

                        function setOrientation() {
                            if (width < topMenu.width) {
                                bottomMenu.state = "portrait"
                            }
                            else bottomMenu.state = "landscape"
                        }

                        Component.onCompleted: {
                            setOrientation()
                        }

                        onWidthChanged: {
                            setOrientation()
                        }
                    }
                }
            }
        }
    }
}
