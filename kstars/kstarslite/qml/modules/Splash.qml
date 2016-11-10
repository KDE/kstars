// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

import QtQuick 2.6
import QtQuick.Window 2.2
import "../constants"

//Rectangle - to allow z-index ordering (for some reason it doesn't work with plain Item)
Rectangle {
    id: splash
    signal timeout
    state: "Inivisble"

    Image {
        id: splashBG
        source: "../images/splash_bg.jpeg"
        anchors {
            fill: parent
        }

        fillMode: Image.PreserveAspectCrop

        width: sourceSize.width/num.pixelRatio
        height: sourceSize.height/num.pixelRatio

        Image {
            id: kdeLogo
            source: "../images/kde-logo.png"
            anchors {
                right: parent.right
                top: parent.top
                margins: 15
            }
        }

        Image {
            id: splashLogo
            source: "../images/splash.png"
            anchors.centerIn: parent
        }

        Text {
            id: progress
            color: "#999"

            anchors {
                bottom: parent.bottom
                horizontalCenter: parent.horizontalCenter
                margins: 5
            }
        }
    }

    Connections {
        target: KStarsData
        onProgressText: {
            progress.text = text
        }
    }

    Connections {
        target: KStarsLite
        onShowSplash: {
            splash.state = "Visible"
        }
        onDataLoadFinished: {
            splash.timeout()
            splash.state = "Invisible"
        }
    }

    states: [
        State{
            name: "Visible"
            PropertyChanges{target: splash; opacity: 1.0}
            PropertyChanges{target: splash; visible: true}
        },
        State{
            name:"Invisible"
            PropertyChanges{target: splash; opacity: 0.0}
            PropertyChanges{target: splash; visible: false}
        }
    ]

    transitions: [
            Transition {
                from: "Visible"
                to: "Invisible"

                SequentialAnimation{
                   NumberAnimation {
                       target: splash
                       property: "opacity"
                       duration: 800
                       easing.type: Easing.InOutQuad
                   }
                   NumberAnimation {
                       target: splash
                       property: "visible"
                       duration: 0
                   }
                }
            }
        ]
}

