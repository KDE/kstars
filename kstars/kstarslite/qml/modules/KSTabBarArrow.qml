// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Layouts 1.2
import QtQuick.Controls 2.0
import "../constants" 1.0

Button {
    z: 1
    id: arrow
    property string imgSource
    property Item tabBar
    property int flickSpeed: 1000
    state: "Invisible"

    width:image.width*1.5
    Image {
        id: image
        opacity: 0.6
        anchors{
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
        }
        source: imgSource
    }

    background: Rectangle {
        color: "grey"
        opacity: 0.6
    }

    onPressedChanged: {
        if(pressed) {
            arrow.background.opacity = 0.3
            tabBar.contentItem.flick(flickSpeed,0)
        } else {
            arrow.background.opacity = 0.6
        }
    }

    states: [
        State {
            name: "Visible"
            PropertyChanges{target: arrow; opacity: 1.0}
            PropertyChanges{target: arrow; visible: true}
        },
        State {
            name:"Invisible"
            PropertyChanges{target: arrow; opacity: 0.0}
            PropertyChanges{target: arrow; visible: false}
        }
    ]

    transitions: [
            Transition {
                from: "Visible"
                to: "Invisible"

                SequentialAnimation{
                   NumberAnimation {
                       target: arrow
                       property: "opacity"
                       duration: arrow
                       easing.type: Easing.InOutQuad
                   }
                   NumberAnimation {
                       target: arrow
                       property: "visible"
                       duration: 50
                   }
                }
            },
            Transition {
                from: "Invisible"
                to: "Visible"
                SequentialAnimation{
                   NumberAnimation {
                       target: arrow
                       property: "visible"
                       duration: 0
                   }
                   NumberAnimation {
                       target: arrow
                       property: "opacity"
                       duration: 200
                       easing.type: Easing.InOutQuad
                   }
                }
            }
        ]
}
