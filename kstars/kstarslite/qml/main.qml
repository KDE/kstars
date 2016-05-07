import QtQuick 2.4
import QtQuick.Window 2.2
import "modules"
import "constants" 1.0
import skymaplite 1.0

Window {
    id: mainWindow
    visible: false
    width: Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight
    property double shadowBgOpacity
    property int drawersOrder: 2
    property int bgOrder: 1
    property int skyMapOrder: 0
    property int topMenuOrder: 0

    property Splash splash: Splash {
        onTimeout: mainWindow.visible = true
    }

    Rectangle {
        id: skyMapLiteWrapper
        color: "#000"
        objectName: "skyMapLiteWrapper"
        anchors.fill: parent
    }

    GlobalDrawer{
        id: globalDrawer
        width: parent.width < parent.height ? parent.width * 0.65  + dragWidth: parent.width * 0.4  + dragWidth
        z: drawersOrder
        anchors {
            top: parent.top
            bottom: parent.bottom
        }
    }

    /*Image {
        anchors.fill: parent
        z: skyMapOrder
        source: "modules/images/SkyMap.png"
        fillMode: Image.PreserveAspectCrop

        Connections {
            target: mainWindow
            onWidthChanged: {
                if (width > height) {
                    source = "images/SkyMap-port.png"
                }
                else source = "images/SkyMap.png"
            }
        }
    }*/

    // The instance of SkyMapLite is reparanted to this item and fill it

    //SkyMapLite {
        //anchors.fill: parent
    //}

    MouseArea {
        property int posY
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        height: parent.height * 0.25

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
    Rectangle {
        id: shadowBg
        z: bgOrder
        anchors.fill: parent
        color: "black"
        opacity: 0
        property double minOpacity: 0.35

        Connections {
            target: globalDrawer
            onBgOpacityChanged: {
                var bgOpacity = globalDrawer.bgOpacity
                if (bgOpacity < shadowBg.minOpacity && contextDrawer.state != "open") shadowBg.opacity = bgOpacity
            }
            onStateChanged: {
                if(globalDrawer.state == "open") contextDrawer.state = ""
            }
        }

        Connections {
            target: contextDrawer
            onBgOpacityChanged: {
                var bgOpacity = contextDrawer.bgOpacity
                if (bgOpacity < shadowBg.minOpacity && globalDrawer.state != "open") shadowBg.opacity = bgOpacity
            }
            onStateChanged: {
                if(contextDrawer.state == "open") globalDrawer.state = ""
            }
        }

        MouseArea {
            anchors.fill: parent
            onPressed: {
                if(globalDrawer.state == "open") {
                    globalDrawer.state = ""
                    mouse.accepted = true
                    return
                }
                if(contextDrawer.state == "open") {
                    contextDrawer.state = ""
                    mouse.accepted = true
                    return
                }
                mouse.accepted = false
            }
        }

        Behavior on opacity {
            PropertyAnimation {
                duration: 300
            }
        }
    }

    MouseArea {
        property int posY
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: parent.height *0.25

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

    ContextDrawer{
        id: contextDrawer
        width: parent.width < parent.height ? parent.width * 0.65  + dragWidth: parent.width * 0.4  + dragWidth
        z: drawersOrder
        anchors {
            top: parent.top
            bottom: parent.bottom
        }
    }
}
