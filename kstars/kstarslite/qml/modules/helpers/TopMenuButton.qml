// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Controls 2.0
import QtGraphicalEffects 1.0
import "../../constants/" 1.0

AbstractButton {
    id: button
    property string iconSrc: ""
    width: icon.width + 5
    height: icon.height + 5
    property bool toggled: false
    opacity: toggled ? 1 : 0.3

    property string title: " "
    property bool titlePlural: true

    onClicked: {
        toggled = !toggled
    }

    onToggledChanged: {
        if(isLoaded) { //Disable while loading
            if(toggled) {
                if(titlePlural) {
                    skyMapLite.notification.showNotification(xi18n("%1 are toggled on", title))
                } else {
                    skyMapLite.notification.showNotification(xi18n("%1 is toggled on", title))
                }
            } else {
                if(titlePlural) {
                    skyMapLite.notification.showNotification(xi18n("%1 are toggled off", title))
                } else {
                    skyMapLite.notification.showNotification(xi18n("%1 is toggled off", title))
                }
            }
        }
    }

    DropShadow {
        anchors.fill: iconRect
        radius: 8.0
        samples: 16
        horizontalOffset: 0
        verticalOffset: 0
        color: "#000000"
        source: iconRect
        opacity: pressed ? 1 : 0

        Behavior on opacity {
            OpacityAnimator { duration: 100 }
        }
    }

    Image {
        id: icon
        source: iconSrc
        width: sourceSize.width/Num.pixelRatio //FIX /Num.pixelRatio we don't need it here!
        height: sourceSize.height/Num.pixelRatio
        anchors.centerIn: iconRect
    }

    background: Rectangle {
        id: iconRect
        radius: 5
        anchors {
            fill: parent
        }
        color: "black"
        border {
            color: toggled ? Num.sysPalette.highlight : "grey"
            width: 1
        }
    }

    onDownChanged: {
        /*if(down) opacity = 0.6
        else opacity = 1*/
    }

    onPressed: {
        //opacity = 0.6
    }

    onReleased: {
        //opacity = 1
    }

    Behavior on opacity {
        OpacityAnimator { duration: 100 }
    }
}
