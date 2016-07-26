/*
 *   Copyright 2015 Marco Martin <mart@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick 2.1
import QtQuick.Layouts 1.2
import QtGraphicalEffects 1.0
import org.kde.kirigami 1.0

import "../templates/private"

Item {
    id: root
    anchors {
        left: parent.left
        right: parent.right
        bottom:parent.bottom
    }
    //smallSpacing for the shadow
    height: button.height + Units.smallSpacing
    clip: true

    //either Action or QAction should work here
    property QtObject action: pageStack.currentItem ? pageStack.currentItem.mainAction : null
    property QtObject leftAction: pageStack.currentItem ? pageStack.currentItem.leftAction : null
    property QtObject rightAction: pageStack.currentItem ? pageStack.currentItem.rightAction : null

    onWidthChanged: button.x = root.width/2 - button.width/2

    transform: Translate {
        id: translateTransform
    }

    Item {
        id: button

        anchors {
            bottom: parent.bottom
            bottomMargin: Units.smallSpacing
        }
        implicitWidth: implicitHeight + Units.iconSizes.smallMedium*3
        implicitHeight: Units.iconSizes.large + Units.largeSpacing


        onXChanged: {
            if (mouseArea.pressed || edgeMouseArea.pressed || fakeContextMenuButton.pressed) {
                if (globalDrawer && globalDrawer.enabled && globalDrawer.modal) {
                    globalDrawer.position = Math.min(1, Math.max(0, (x - root.width/2 + button.width/2)/globalDrawer.contentItem.width + mouseArea.drawerShowAdjust));
                }
                if (contextDrawer && contextDrawer.enabled && contextDrawer.modal) {
                    contextDrawer.position = Math.min(1, Math.max(0, (root.width/2 - button.width/2 - x)/contextDrawer.contentItem.width + mouseArea.drawerShowAdjust));
                }
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent

            visible: action != null || leftAction != null || rightAction != null
            property bool internalVisibility: (applicationWindow === undefined || (applicationWindow().controlsVisible && applicationWindow().height > root.height*2)) && (root.action === null || root.action.visible === undefined || root.action.visible)
            preventStealing: true
            onInternalVisibilityChanged: {
                showAnimation.running = false;
                if (internalVisibility) {
                    showAnimation.to = 0;
                } else {
                    showAnimation.to = button.height;
                }
                showAnimation.running = true;
            }

            drag {
                target: button
                //filterChildren: true
                axis: Drag.XAxis
                minimumX: contextDrawer && contextDrawer.enabled && contextDrawer.modal ? 0 : root.width/2 - button.width/2
                maximumX: globalDrawer && globalDrawer.enabled && globalDrawer.modal ? root.width : root.width/2 - button.width/2
            }

            property var downTimestamp;
            property int startX
            property int startMouseY
            property real drawerShowAdjust
            property bool buttonPressedUnderMouse: false
            property bool leftButtonPressedUnderMouse: false
            property bool rightButtonPressedUnderMouse: false

            onPressed: {
                //search if we have a page to set to current
                if (applicationWindow !== undefined && applicationWindow().pageStack.currentIndex !== undefined) {
                    //search the button parent's parent, that is the page parent
                    //this will make the context drawer open for the proper page
                    applicationWindow().pageStack.currentIndex = root.parent.parent.parent.level;
                }
                downTimestamp = (new Date()).getTime();
                startX = button.x + button.width/2;
                startMouseY = mouse.y;
                drawerShowAdjust = 0;
                buttonPressedUnderMouse = mouse.x > buttonGraphics.x && mouse.x < buttonGraphics.x + buttonGraphics.width;
                leftButtonPressedUnderMouse = !buttonPressedUnderMouse && leftAction && mouse.x < buttonGraphics.x;
                rightButtonPressedUnderMouse = !buttonPressedUnderMouse && rightAction && mouse.x > buttonGraphics.x + buttonGraphics.width;
            }
            onReleased: {
                //pixel/second
                var x = button.x + button.width/2;
                var speed = ((x - startX) / ((new Date()).getTime() - downTimestamp) * 1000);
                drawerShowAdjust = 0;

                //project where it would be a full second in the future
                if (globalDrawer && globalDrawer.modal && x + speed > Math.min(root.width/4*3, root.width/2 + globalDrawer.contentItem.width/2)) {
                    globalDrawer.open();
                    contextDrawer.close();
                } else if (contextDrawer && x + speed < Math.max(root.width/4, root.width/2 - contextDrawer.contentItem.width/2)) {
                    if (contextDrawer && contextDrawer.modal) {
                        contextDrawer.open();
                    }
                    if (globalDrawer && globalDrawer.modal) {
                        globalDrawer.close();
                    }
                } else {
                    if (globalDrawer && globalDrawer.modal) {
                        globalDrawer.close();
                    }
                    if (contextDrawer && contextDrawer.modal) {
                        contextDrawer.close();
                    }
                }
                //Don't rely on native onClicked, but fake it here:
                //Qt.startDragDistance is not adapted to devices dpi in case
                //of Android, so consider the button "clicked" when:
                //*the button has been dragged less than a gridunit
                //*the finger is still on the button
                if (Math.abs((button.x + button.width/2) - startX) < Units.gridUnit &&
                    mouse.y > 0) {
                    var action;
                    if (buttonPressedUnderMouse) {
                        action = root.action;
                    } else if (leftButtonPressedUnderMouse) {
                        action = root.leftAction;
                    } else if (rightButtonPressedUnderMouse) {
                        action = root.rightAction;
                    }

                    if (!action) {
                        return;
                    }

                    //if an action has been assigned, trigger it
                    if (action && action.trigger) {
                        action.trigger();
                    }
                }
            }

            onPositionChanged: {
                drawerShowAdjust = Math.min(0.3, Math.max(0, (startMouseY - mouse.y)/(Units.gridUnit*15)));
                button.xChanged();
            }
            onPressAndHold: {
                var action;
                if (buttonPressedUnderMouse) {
                    action = root.action;
                } else if (leftButtonPressedUnderMouse) {
                    action = root.leftAction;
                } else if (rightButtonPressedUnderMouse) {
                    action = root.rightAction;
                }

                if (!action) {
                    return;
                }

                //if an action has been assigned, show a message like a tooltip
                if (action && action.text) {
                    showPassiveNotification(action.text);
                }
            }
            Connections {
                target: globalDrawer
                onPositionChanged: {
                    if ( globalDrawer && globalDrawer.modal && !mouseArea.pressed && !edgeMouseArea.pressed && !fakeContextMenuButton.pressed) {
                        button.x = globalDrawer.contentItem.width * globalDrawer.position + root.width/2 - button.width/2;
                    }
                }
            }
            Connections {
                target: contextDrawer
                onPositionChanged: {
                    if (contextDrawer && contextDrawer.modal && !mouseArea.pressed && !edgeMouseArea.pressed && !fakeContextMenuButton.pressed) {
                        button.x = root.width/2 - button.width/2 - contextDrawer.contentItem.width * contextDrawer.position;
                    }
                }
            }

            NumberAnimation {
                id: showAnimation
                target: translateTransform
                properties: "y"
                duration: Units.longDuration
                easing.type: mouseArea.internalVisibility == true ? Easing.InQuad : Easing.OutQuad
            }
            Item {
                id: background
                anchors {
                    fill: parent
                }

                Rectangle {
                    id: buttonGraphics
                    radius: width/2
                    anchors.centerIn: parent
                    height: parent.height - Units.smallSpacing*2
                    width: height
                    visible: root.action
                    readonly property bool pressed: root.action && ((mouseArea.buttonPressedUnderMouse && mouseArea.pressed) || root.action.checked)
                    color: pressed ? Theme.highlightColor : Theme.backgroundColor

                    Icon {
                        id: icon
                        source: root.action && root.action.iconName ? root.action.iconName : ""
                        selected: buttonGraphics.pressed
                        anchors {
                            fill: parent
                            margins: Units.smallSpacing
                        }
                    }
                    Behavior on color {
                        ColorAnimation {
                            duration: Units.longDuration
                            easing.type: Easing.InOutQuad
                        }
                    }
                    Behavior on x {
                        NumberAnimation {
                            duration: Units.longDuration
                            easing.type: Easing.InOutQuad
                        }
                    }
                }
                //left button
                Rectangle {
                    id: leftButtonGraphics
                    z: -1
                    anchors {
                        left: parent.left
                        verticalCenter: parent.verticalCenter
                    }
                    radius: Units.smallSpacing
                    height: buttonGraphics.height * 0.7
                    width: height + (root.action ? Units.iconSizes.smallMedium : 0)
                    visible: root.leftAction

                    readonly property bool pressed: root.leftAction && ((mouseArea.leftButtonPressedUnderMouse && mouseArea.pressed) || root.leftAction.checked)
                    color: pressed ? Theme.highlightColor : Theme.backgroundColor
                    Icon {
                        source: root.leftAction && root.leftAction.iconName ? root.leftAction.iconName : ""
                        width: height
                        selected: leftButtonGraphics.pressed
                        anchors {
                            left: parent.left
                            top: parent.top
                            bottom: parent.bottom
                            margins: Units.smallSpacing
                        }
                    }
                }
                //right button
                Rectangle {
                    id: rightButtonGraphics
                    z: -1
                    anchors {
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    radius: Units.smallSpacing
                    height: buttonGraphics.height * 0.7
                    width: height + (root.action ? Units.iconSizes.smallMedium : 0)
                    visible: root.rightAction
                    readonly property bool pressed: root.rightAction && ((mouseArea.rightButtonPressedUnderMouse && mouseArea.pressed) || root.rightAction.checked)
                    color: pressed ? Theme.highlightColor : Theme.backgroundColor
                    Icon {
                        source: root.rightAction && root.rightAction.iconName ? root.rightAction.iconName : ""
                        width: height
                        selected: rightButtonGraphics.pressed
                        anchors {
                            right: parent.right
                            top: parent.top
                            bottom: parent.bottom
                            margins: Units.smallSpacing
                        }
                    }
                }
            }

            DropShadow {
                anchors.fill: background
                horizontalOffset: 0
                verticalOffset: Units.smallSpacing/3
                radius: Units.gridUnit / 3.5
                samples: 16
                color: Qt.rgba(0, 0, 0, 0.5)
                source: background
            }
        }
    }

    MouseArea {
        id: fakeContextMenuButton
        anchors {
            right: edgeMouseArea.right
            bottom: edgeMouseArea.bottom
            margins: -1
        }
        drag {
            target: button
            //filterChildren: true
            axis: Drag.XAxis
            minimumX: contextDrawer && contextDrawer.enabled && contextDrawer.modal ? 0 : root.width/2 - button.width/2
            maximumX: globalDrawer && globalDrawer.enabled && globalDrawer.modal ? root.width : root.width/2 - button.width/2
        }
        visible: root.parent.parent.actions.contextualActions.length > 0 && (applicationWindow === undefined || applicationWindow().wideScreen)

        width: Units.iconSizes.medium
        height: width

        Rectangle {
            color: Theme.viewBackgroundColor
            opacity: parent.pressed ? 1 : 0.3
            anchors.fill: parent
        }

        ContextIcon {
            anchors.centerIn: parent
            width: height
            height: Units.iconSizes.smallMedium - Units.smallSpacing * 2
        }

        onPressed: mouseArea.onPressed(mouse)
        onReleased: {
            var pos = root.mapFromItem(fakeContextMenuButton, mouse.x, mouse.y);
            if (pos.x < root.width/2 || (mouse.x > 0 && mouse.x < width)) {
                contextDrawer.open();
            } else {
                contextDrawer.close();
            }
            if (globalDrawer.position > 0.5) {
                globalDrawer.open();
            } else {
                globalDrawer.close();
            }
        }
        layer.enabled: true
        layer.effect: DropShadow {
            horizontalOffset: 0
            verticalOffset: 0
            radius: Units.gridUnit
            samples: 32
            color: Qt.rgba(0, 0, 0, 0.5)
        }
    }

    MouseArea {
        id: edgeMouseArea
        z:99
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        drag {
            target: button
            //filterChildren: true
            axis: Drag.XAxis
            minimumX: contextDrawer && contextDrawer.enabled && contextDrawer.modal ? 0 : root.width/2 - button.width/2
            maximumX: globalDrawer && globalDrawer.enabled && globalDrawer.modal ? root.width : root.width/2 - button.width/2
        }
        height: Units.smallSpacing * 3

        onPressed: mouseArea.onPressed(mouse)
        onPositionChanged: mouseArea.positionChanged(mouse)
        onReleased: mouseArea.released(mouse)
    }
}
