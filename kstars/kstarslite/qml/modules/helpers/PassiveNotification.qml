/*
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick 2.5
import QtQuick.Controls 2.0 as Controls
import QtQuick.Layouts 1.2
import QtGraphicalEffects 1.0
import "../../constants" 1.0
import "../../modules"

MouseArea {
    id: root
    z: 9999
    width: background.width
    height: background.height
    opacity: 0
    enabled: appearAnimation.appear

    anchors {
        horizontalCenter: parent.horizontalCenter
        bottom: parent.bottom
        bottomMargin: units.gridUnit * 4
    }

    function showNotification(message, timeout, actionText, callBack) {
        if (!message) {
            return;
        }
        appearAnimation.running = false;
        appearAnimation.appear = true;
        appearAnimation.running = true;
        if (timeout == "short") {
            timer.interval = 1000;
        } else if (timeout == "long") {
            timer.interval = 4500;
        } else if (timeout > 0) {
            timer.interval = timeout;
        } else {
            timer.interval = 3000;
        }
        messageLabel.text = message ? message : "";
        actionButton.text = actionText ? actionText : "";
        actionButton.callBack = callBack ? callBack : "";

        timer.restart();
    }

    function hideNotification() {
        appearAnimation.running = false;
        appearAnimation.appear = false;
        appearAnimation.running = true;
    }


    onClicked: {
        appearAnimation.appear = false;
        appearAnimation.running = true;
    }

    transform: Translate {
        id: transform
        y: root.height
    }

    Timer {
        id: timer
        interval: 4000
        onTriggered: {
            appearAnimation.appear = false;
            appearAnimation.running = true;
        }
    }
    ParallelAnimation {
        id: appearAnimation
        property bool appear: true
        NumberAnimation {
            target: root
            properties: "opacity"
            to: appearAnimation.appear ? 1 : 0
            duration: units.longDuration
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: transform
            properties: "y"
            to: appearAnimation.appear ? 0 : background.height
            duration: units.longDuration
            easing.type: appearAnimation.appear ? Easing.OutQuad : Easing.InQuad
        }
    }

    Item {
        id: background
        width: backgroundRect.width + units.gridUnit
        height: backgroundRect.height + units.gridUnit

        Rectangle {
            id: backgroundRect
            anchors.centerIn: parent
            radius: units.smallSpacing
            color: Num.sysPalette.base
            border.color: Num.sysPalette.light
            border.width: 1
            opacity: 0.6
            width: mainLayout.width + Math.round((height - mainLayout.height))
            height: Math.max(mainLayout.height + units.smallSpacing*2, units.gridUnit*2)
        }

        RowLayout {
            id: mainLayout
            anchors.centerIn: parent
            KSLabel {
                id: messageLabel
                width: Math.min(root.parent.width - units.largeSpacing*2, implicitWidth)
            }
            Controls.Button {
                id: actionButton
                property var callBack
                visible: text != ""
                onClicked: {
                    appearAnimation.appear = false;
                    appearAnimation.running = true;
                    if (callBack) {
                        callBack();
                    }
                }
            }
        }

        layer.enabled: true
        layer.effect: DropShadow {
            horizontalOffset: 0
            verticalOffset: 0
            radius: units.gridUnit
            samples: 32
            color: Num.sysPalette.shadow//Qt.rgba(0, 0, 0, 0.5)
        }
    }
}

