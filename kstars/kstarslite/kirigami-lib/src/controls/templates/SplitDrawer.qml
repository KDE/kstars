/*
 *   Copyright 2012 Marco Martin <mart@kde.org>
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
import org.kde.kirigami 1.0
import "private"

/**
 * Split Drawers are used to expose additional UI elements which are optional
 * and can be used in conjunction with the main UI elements.
 * For example the Resource Browser uses a Split Drawer to select
 * different kinds of filters for the main view.
 */
AbstractDrawer {
    id: root
    z:0

    visible: true

    /**
     * page: Item
     * It's the default property. it's the main content of the drawer page,
     * the part that is always shown
     */
    default property alias page: mainPage.data

    /**
     * contentItem: Item
     * It's the part that can be pulled in and out, will act as a sidebar.
     */
    property Item contentItem

    /**
     * opened: bool
     * If true the drawer is open showing the contents of the "drawer"
     * component.
     */
    property alias opened: sidebar.open

    /**
     * position: real
     * This property holds the position of the drawer relative to its
     * final destination. That is, the position will be 0 when the
     * drawer is fully closed, and 1 when fully open.
     */
    property real position: 0

    /**
     * modal: bool
     * If true the drawer will be an overlay of the main content,
     * obscuring it and blocking input.
     * If false, the drawer will look like a sidebar, with the main content
     * application still usable.
     * It is recomended to use modal on mobile devices and not modal on desktop
     * devices.
     * Default is true
     */
    //property bool modal: true

    /**
     * background: Item
     * This property holds the background item.
     *
     * Note: If the background item has no explicit size specified,
     * it automatically follows the control's size.
     * In most cases, there is no need to specify width or
     * height for a background item.
     */
    property Item background

    onBackgroundChanged: {
        background.parent = browserFrame;
        background.anchors.fill = browserFrame;
        background.z = -1;
    }

    Component.onCompleted: {
        mainPage.width = browserFrame.width
        contentItem.parent = drawerPage
    }

    onPositionChanged: {
        if (!browserFrame.loopCheck) {
            browserFrame.loopCheck = true;
            browserFrame.x = position * drawerPage.width;
            browserFrame.loopCheck = false;
        }
    }
    onContentItemChanged: contentItem.parent = drawerPage
    MouseArea {
        id: mouseEventListener
        z: 200
        drag.filterChildren: true
        //drag.target: browserFrame
        property int startMouseX
        property int oldMouseX
        property int startBrowserFrameX
        property bool toggle: false
        property string startState
        enabled: root.modal

        anchors.fill: parent

        onPressed: {
            if (drawerPage.children.length == 0 || (browserFrame.state == "Closed" && mouse.x > Units.gridUnit) ||
                mouse.x < browserFrame.x) {
                mouse.accepted = false;
                return;
            }

            toggle = true;
            startBrowserFrameX = browserFrame.x;
            oldMouseX = startMouseX = mouse.x;
            startState = browserFrame.state;
            browserFrame.state = "Dragging";
            browserFrame.x = startBrowserFrameX;
        }

        onPositionChanged: {
            browserFrame.x = Math.max(0, browserFrame.x + mouse.x - oldMouseX);
            oldMouseX = mouse.x;
            if (Math.abs(mouse.x - startMouseX) > Units.gridUnit * 2) {
                toggle = false;
            }
        }

        onReleased: {
            if (toggle) {
                browserFrame.state = startState == "Open" ? "Closed" : "Open"
            } else if (browserFrame.x < drawerPage.width / 2) {
                browserFrame.state = "Closed";
            } else {
                browserFrame.state = "Open";
            }
        }
        onClicked: root.clicked()
    }

    Item {
        id: browserFrame
        z: 100
        state: sidebar.open ? "Open" : "Closed"
        onStateChanged: sidebar.open = (state != "Closed")
        readonly property real position: Math.abs(x) / drawerPage.width
        property bool loopCheck: false
        onPositionChanged: {
            if (!loopCheck) {
                loopCheck = true;
                root.position = position;
                loopCheck = false;
            }
        }

        anchors {
            top: parent.top
            bottom: parent.bottom
        }
        width: root.width;

        Item {
            id: mainPage
            onChildrenChanged: mainPage.children[0].anchors.fill = mainPage

            anchors.fill: parent
        }

        Rectangle {
            anchors.fill: parent
            color: "black"
            opacity: Math.min(0.4, 0.4 * (browserFrame.x / drawerPage.width))
            visible: root.modal
        }

        states: [
            State {
                name: "Open"
                PropertyChanges {
                    target: browserFrame
                    x: drawerPage.width
                }

            },
            State {
                name: "Dragging"
                PropertyChanges {
                    target: browserFrame
                    x: mouseEventListener.startBrowserFrameX
                }
            },
            State {
                name: "Closed"
                PropertyChanges {
                    target: browserFrame
                    x: 0
                }
            }
        ]

        transitions: [
            Transition {
                //Exclude Dragging
                to: "Open,Closed,Hidden"
                NumberAnimation {
                    properties: "x"
                    duration: Units.longDuration
                    easing.type: Easing.InOutQuad
                }
            }
        ]
    }


    Item {
        id: sidebar

        property bool open: false
        onOpenChanged: {
            if (drawerPage.children.length == 0) {
                return;
            }

            if (open) {
                browserFrame.state = "Open";
            } else {
                browserFrame.state = "Closed";
            }
        }

        width: browserFrame.x
        clip: true

        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
        }

        Item {
            id: drawerPage
            width: Math.min(root.width/4*3, Math.max(root.contentItem ? root.contentItem.implicitWidth : 0, root.width/4))
            anchors {
                top: parent.top
                bottom: parent.bottom
                left: parent.left
                topMargin: (applicationWindow !== undefined && applicationWindow().header) && modal ? applicationWindow().header.height : 0
            }
            clip: false
            onChildrenChanged: drawerPage.children[0].anchors.fill = drawerPage
        }
    }
}

