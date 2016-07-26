/*
 *   Copyright 2015 Marco Martin <mart@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
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

import QtQuick 2.5
import QtQuick.Layouts 1.2
import "private"
import org.kde.kirigami 1.0


/**
 * An item that can be used as a title for the application.
 * Scrolling the main page will make it taller or shorter (trough the point of going away)
 * It's a behavior similar to the typical mobile web browser adressbar
 * the minimum, preferred and maximum heights of the item can be controlled with
 * * minimumHeight: default is 0, i.e. hidden
 * * preferredHeight: default is Units.gridUnit * 1.6
 * * preferredHeight: default is Units.gridUnit * 3
 *
 * To achieve a titlebar that stays completely fixed just set the 3 sizes as the same
 */
Item {
    id: root
    z: 90
    property int minimumHeight: 0
    property int preferredHeight: Units.gridUnit * 2
    property int maximumHeight: Units.gridUnit * 3
    default property alias contentItem: mainItem.data

    parent: __appWindow.contentItem.parent
    //FIXME: remove
    property QtObject __appWindow: applicationWindow();

    anchors {
        top: parent.top
        left: parent.left
        right: parent.right
    }
    height: preferredHeight

    /**
     * background: Item
     * This property holds the background item.
     * Note: the background will be automatically sized as the whole control
     */
    property Item background

    onBackgroundChanged: {
        background.z = -1;
        background.parent = root;
        background.anchors.fill = root;
    }

    Behavior on height {
        enabled: __appWindow.pageStack.currentItem && __appWindow.pageStack.currentItem.flickable && !__appWindow.pageStack.currentItem.flickable.moving
        NumberAnimation {
            duration: Units.longDuration
            easing.type: Easing.InOutQuad
        }
    }

    opacity: height > 0 && -translateTransform.y <= height ? 1 : 0
    Behavior on opacity {
        OpacityAnimator {
            duration: Units.longDuration
            easing.type: Easing.InOutQuad
        }
    }

    Connections {
        target: __appWindow
        onWideScreenChanged: {
            if (wideScreen) {
                height = preferredHeight;
            } else {
                height = preferredHeight;
            }
        }
        onHeightChanged: root.height = preferredHeight;
        onReachableModeChanged: root.height = __appWindow.reachableMode  && !__appWindow.wideScreen ? maximumHeight : preferredHeight;
    }

    transform: Translate {
        id: translateTransform
        y: {
            if (__appWindow === undefined) {
                return 0;
            }
            if (__appWindow.reachableMode && !__appWindow.wideScreen) {
                return __appWindow.height/2;
            } else if (!__appWindow.controlsVisible) {
                return -headerItem.height - Units.smallSpacing;
            } else {
                return 0;
            }
        }
        Behavior on y {
            NumberAnimation {
                duration: Units.longDuration
                easing.type: translateTransform.y < 0 ? Easing.OutQuad : Easing.InQuad
            }
        }
    }

    Item {
        id: headerItem
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        height: parent.preferredHeight

        Connections {
            id: headerSlideConnection
            target: __appWindow.pageStack.currentItem ? __appWindow.pageStack.currentItem.flickable : null
            property int oldContentY
            onContentYChanged: {
                if (!__appWindow.pageStack.currentItem) {
                    return;
                }
                if (__appWindow.pageStack.currentItem.flickable.atYBeginning ||
                    __appWindow.pageStack.currentItem.flickable.atYEnd) {
                    return;
                }

                if (__appWindow.wideScreen) {
                    root.height = root.preferredHeight;
                } else if (__appWindow.reachableMode && !__appWindow.wideScreen) {
                    root.height = root.maximumHeight;
                } else {
                    root.height = Math.min(root.preferredHeight,
                                           Math.max(root.minimumHeight,
                                               root.height + oldContentY - __appWindow.pageStack.currentItem.flickable.contentY));
                    oldContentY = __appWindow.pageStack.currentItem.flickable.contentY;
                }
            }
            onMovementEnded: {
                if (root.height > root.preferredHeight) {
                    //if don't change the position if more then preferredSize is shown
                } else if (root.height > root.preferredHeight/2 ) {
                    root.height = root.preferredHeight;
                } else {
                    root.height = 0;
                }
            }
        }
        Connections {
            target: __appWindow.pageStack
            onCurrentItemChanged: {
                if (!__appWindow.pageStack.currentItem) {
                    return;
                }
                if (__appWindow.pageStack.currentItem.flickable) {
                    headerSlideConnection.oldContentY = __appWindow.pageStack.currentItem.flickable.contentY;
                } else {
                    headerSlideConnection.oldContentY = 0;
                }
                root.height = root.preferredHeight;
            }
        }

        Item {
            id: mainItem
            anchors {
                fill: parent
                topMargin: applicationWindow().reachable ? 0 : Math.min(headerItem.height - root.height, headerItem.height - root.preferredHeight)
            }
        }
    }
}

