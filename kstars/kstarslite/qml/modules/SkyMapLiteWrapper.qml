// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Controls 2.0

import "../constants" 1.0
import "helpers"
import "tutorial"

Item {
    id: skyMapLiteItem
    visible: isLoaded
    property alias notification: notification

    Rectangle {
        id: skyMapLiteWrapper
        objectName: "skyMapLiteWrapper"
        width: parent.width
        height: parent.height
        color: KStarsLite.getColor("SkyColor")

        Connections {
            target: colorSchemePopup
            onColorSchemeChanged: {
                skyMapLiteWrapper.color = KStarsLite.getColor("SkyColor")
            }
        }

        Connections {
            target: stackView
            onCurrentItemChanged: {
                if(stackView.currentItem != initPage) {
                    //Workaround to make animation on push from / and pop to initPage faster
                    //skyMapLiteWrapper.anchors.fill = null
                    skyMapLiteWrapper.width = 0
                    skyMapLiteWrapper.height = 0
                    skyMapLite.visible = false
                }
            }
            onBusyChanged: {
                if(stackView.currentItem == initPage) {
                    skyMapLite.visible = true
                    skyMapLiteWrapper.width = Qt.binding(function() {return skyMapLite.width})
                    skyMapLiteWrapper.height = Qt.binding(function() {return skyMapLite.height})
                }
            }
        }

        Button {
            z: 1
            visible: SkyMapLite.centerLocked
            anchors {
                right: parent.right
                top: parent.top
                margins: 25
            }
            onClicked: {
                SkyMapLite.centerLocked = false
            }
            onPressedChanged: {
                if(pressed)
                    lockBG.color = "#D40000"
                else
                    lockBG.color = "red"
            }

            background: Rectangle {
                id: lockBG
                color: "red"
                implicitWidth: 100
                implicitHeight: 70
                radius: 4
                border.width: 3
                border.color: "#AA0000"
            }

            Image {
                source: "../images/lock-closed.png"
                anchors.centerIn: parent
            }
        }

        Button {
            z: 1
            id: exitAutomaticMode
            visible: SkyMapLite.automaticMode
            anchors {
                left: parent.left
                top: parent.top
                margins: 25
            }

            Image {
                source: "../images/back.png"
                anchors.centerIn: parent
            }

            onClicked: {
                SkyMapLite.automaticMode = false
            }
        }

        /** Circle that appears after user taps on screen **/
        Rectangle {
            id: tapCircle
            z: 1
            width: 20 * Num.dp
            radius: width*0.5
            height: width
            color: "grey"
            opacity: 0

            Connections {
                target: SkyMapLite
                onPosClicked: {
                    tapCircle.x = pos.x - tapCircle.width * 0.5
                    tapCircle.y = pos.y - tapCircle.height * 0.5
                    tapAnimation.start()
                }
                onPointLiteChanged: {
                    contextMenu.openPoint()
                }

                onObjectLiteChanged: {
                    contextMenu.openObject()
                }
            }

            SequentialAnimation on opacity {
                id: tapAnimation
                OpacityAnimator { from: 0; to: 0.8; duration: 100 }
                OpacityAnimator { from: 0.8; to: 0; duration: 400 }
            }
        }
    }

    TopMenu {
        id: topMenu
    }

    //Step 3 - Top Menu
    TutorialStep3 {
        anchors {
            top: topMenu.bottom
        }
    }

    PassiveNotification {
        z: 2
        height: 10
        id: notification
        visible: true
            anchors {
                bottom: bottomMenu.top
            horizontalCenter: parent.horizontalCenter
            }

        Connections {
            target: KStarsLite
            onNotificationMessage: {
                skyMapLite.notification.showNotification(msg)
            }
        }
    }

    BottomMenu {
        id: bottomMenu
    }

    //Step 4 - Bottom Menu
    TutorialStep4 {
        anchors{
            bottom: bottomMenu.top
        }
    }
}
