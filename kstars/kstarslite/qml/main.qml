import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import "modules"
import "indi"
import "constants" 1.0
import QtQuick.Controls 1.4 as Controls
import org.kde.kirigami 1.0 as Kirigami
import QtSensors 5.0

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

    pageStack.initialPage: null

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

        Kirigami.Label {
            text: "Magnitude Limit"
        }

        Controls.Slider {
            Layout.fillWidth: true

            maximumValue: 5.75954
            minimumValue: 1.18778
            value: SkyMapLite.magLim
            onValueChanged: {
                SkyMapLite.magLim = value
            }
        }
    }

    contextDrawer: Kirigami.ContextDrawer {
        id: contextDrawer
        property Item contextMenu

        actions: initPage.actions.contextualActions

        onOpenedChanged: {
            if(opened) {
                //If no telescopes are connected then we disable actions in context drawer
                var areTelescopesLoaded = false
                if(ClientManagerLite.connected) {
                    for(var i = 0; i < mainWindow.telescopes.length; ++i) {
                        if(mainWindow.telescopes[i].isConnected()) {
                            areTelescopesLoaded = true
                        }
                    }
                }
                initPage.telescopeLoaded = areTelescopesLoaded
            }
        }

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

        TapSensor {
            onReadingChanged: {
                console.log(reading.doubleTap)
            }
        }

        onVisibleChanged: {
            if(visible) {
                SkyMapLite.update() //Update SkyMapLite once user opened initPage
            }
        }

        property bool isClickedObject: false
        property bool telescopeLoaded: false

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

        Connections {
            target: ClientManagerLite
            /*onDeviceConnected: {
                var isConnected = false
                for(var i = 0; i < telescopes.length; ++i) {
                    if(telescopes[i].isConnected()) {
                        isConnected = true
                    }
                }
                initPage.telescopeLoaded = Connected
            }*/
            onTelescopeAdded: {
                telescopes.push(newTelescope)
                initPage.telescopeLoaded = true
            }
        }

        actions {
            contextualActions: [
                Kirigami.Action {
                    enabled: initPage.telescopeLoaded
                    text: "Slew to object"
                    onTriggered: {
                        for(var i = 0; i < telescopes.length; ++i) {
                            if(telescopes[i].isConnected()) {
                                if(initPage.isClickedObject) {
                                    telescopes[i].slew(ClickedObject)
                                } else {
                                    telescopes[i].slew(ClickedPoint)
                                }
                            }
                        }
                    }
                },
                Kirigami.Action {
                    enabled: initPage.telescopeLoaded
                    text: "Sync"
                    onTriggered: {
                        for(var i = 0; i < telescopes.length; ++i) {
                            if(telescopes[i].isConnected()) {
                                if(initPage.isClickedObject) {
                                    telescopes[i].sync(ClickedObject)
                                } else {
                                    telescopes[i].sync(ClickedPoint)
                                }
                            }
                        }
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

                Rectangle {
                     id: tapCircle
                     z: 1
                     width: 20 * num.dp
                     height: width
                     color: "grey"
                     radius: width*0.5
                     opacity: 0

                     Connections {
                         target: SkyMapLite
                         onPosClicked: {
                            tapCircle.x = pos.x - tapCircle.width * 0.5
                            tapCircle.y = pos.y - tapCircle.height * 0.5
                            tapAnimation.start()
                         }
                     }

                     SequentialAnimation on opacity {
                         id: tapAnimation
                         OpacityAnimator { from: 0; to: 0.8; duration: 100 }
                         OpacityAnimator { from: 0.8; to: 0; duration: 400 }
                     }
                }

                /*MouseArea {
                    z: 1
                    anchors.fill: parent
                    propagateComposedEvents: true
                    onClicked: {
                        tapCircle.x = mouseX - tapCircle.width/2
                        tapCircle.y = mouseY - tapCircle.height/2
                        mouse.accepted = false
                    }
                }*/
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
