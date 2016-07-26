/*
*   Copyright (C) 2016 by Marco Martin <mart@kde.org>
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
*   51 Franklin Street, Fifth Floor, Boston, MA  2.010-1301, USA.
*/

import QtQuick 2.5
import QtQuick.Controls 1.3 as Controls
import org.kde.kirigami 1.0
import QtGraphicalEffects 1.0
import "private"

/**
 * An overlay sheet that covers the current Page content.
 * Its contents can be scrolled up or down, scrolling all the way up or
 * all the way down, dismisses it.
 * Use this for big, modal dialogs or information display, that can't be
 * logically done as a new separate Page, even if potentially
 * are taller than the screen space.
 */
Item {
    id: root

    z: 999

    anchors.fill: parent
    visible: false

    /**
     * contentItem: Item
     * This property holds the visual content item.
     *
     * Note: The content item is automatically resized inside the
     * padding of the control.
     */
    default property Item contentItem

    /**
     * opened: bool
     * If true the sheet is open showing the contents of the OverlaySheet
     * component.
     */
    property bool opened

    /**
     * leftPadding: int
     * default contents padding at left
     */
    property int leftPadding: Units.gridUnit

    /**
     * topPadding: int
     * default contents padding at top
     */
    property int topPadding: Units.gridUnit

    /**
     * rightPadding: int
     * default contents padding at right
     */
    property int rightPadding: Units.gridUnit

    /**
     * bottomPadding: int
     * default contents padding at bottom
     */
    property int bottomPadding: Units.gridUnit

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


    function open() {
        root.visible = true;
        openAnimation.from = -root.height;
        openAnimation.to = openAnimation.topOpenPosition;
        openAnimation.running = true;
        root.opened = true;
    }

    function close() {
        if (mainFlickable.contentY < 0) {
            closeAnimation.to = -height;
        } else {
            closeAnimation.to = flickableContents.height;
        }
        closeAnimation.running = true;
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.textColor
        opacity: 0.6 * Math.min(
            (Math.min(mainFlickable.contentY + mainFlickable.height, mainFlickable.height) / mainFlickable.height),
            (2 + (mainFlickable.contentHeight - mainFlickable.contentY - mainFlickable.topMargin - mainFlickable.bottomMargin)/mainFlickable.height))
    }


    Component.onCompleted: {
        mainFlickable.interactive = true;
    }
    onBackgroundChanged: {
        background.parent = flickableContents;
        background.z = -1;
    }
    onContentItemChanged: {
        contentItem.parent = contentItemParent;
        contentItem.anchors.left = contentItemParent.left;
        contentItem.anchors.right = contentItemParent.right;
    }
    onOpenedChanged: {
        if (opened) {
            open();
        } else {
            close();
            Qt.inputMethod.hide();
        }
    }
    onHeightChanged: {
        var focusItem;

        if (typeof applicationWindow !== "undefined") {
            focusItem = applicationWindow().activeFocusItem;
        //fallback: hope activeFocusItem is in context
        } else {
            focusItem = activeFocusItem;
        }

        if (!activeFocusItem) {
            return;
        }

        //NOTE: there is no function to know if an item is descended from another,
        //so we have to walk the parent hyerarchy by hand
        var isDescendent = false;
        var candidate = focusItem.parent;
        while (candidate) {
            if (candidate == root) {
                isDescendent = true;
                break;
            }
            candidate = candidate.parent;
        }
        if (!isDescendent) {
            return;
        }

        var cursorY = 0;
        if (focusItem.cursorPosition !== undefined) {
            cursorY = focusItem.positionToRectangle(focusItem.cursorPosition).y;
        }

        
        var pos = focusItem.mapToItem(flickableContents, 0, cursorY - Units.gridUnit*3);
        //focused item alreqady visible? add some margin for the space of the action buttons
        if (pos.y >= mainFlickable.contentY && pos.y <= mainFlickable.contentY + mainFlickable.height - Units.gridUnit * 8) {
            return;
        }
        mainFlickable.contentY = pos.y;
    }


    NumberAnimation {
        id: openAnimation
        property int topOpenPosition: Math.min(-root.height*0.15, flickableContents.height - root.height + Units.gridUnit * 5)
        property int bottomOpenPosition: (flickableContents.height - root.height) + (Units.gridUnit * 5)
        target: mainFlickable
        properties: "contentY"
        from: -root.height
        to: topOpenPosition
        duration: Units.longDuration
        easing.type: Easing.OutQuad
    }

    SequentialAnimation {
        id: closeAnimation
        property int to: -root.height
        NumberAnimation {
            target: mainFlickable
            properties: "contentY"
            to: closeAnimation.to
            duration: Units.longDuration
            easing.type: Easing.InQuad
        }
        ScriptAction {
            script: root.visible = root.opened = false;
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 2
        drag.filterChildren: true

        onClicked: {
            var pos = mapToItem(flickableContents, mouse.x, mouse.y);
            if (!flickableContents.contains(pos)) {
                root.close();
            }
        }

        Controls.ScrollView {
            id: scrollView
            anchors.fill: parent
            Flickable {
                id: mainFlickable
                topMargin: height
                contentWidth: width
                contentHeight: flickableContents.height;
                flickableDirection: Flickable.VerticalFlick
                Item {
                    width: mainFlickable.width
                    height: flickableContents.height
                    Item {
                        id: flickableContents
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: root.contentItem.implicitWidth <= 0 ? root.width : Math.max(root.width/2, Math.min(root.width, root.contentItem.implicitWidth))
                        height: contentItem.height + topPadding + bottomPadding + Units.iconSizes.medium + Units.gridUnit
                        Item {
                            id: contentItemParent
                            anchors {
                                fill: parent
                                leftMargin: leftPadding
                                topMargin: topPadding
                                rightMargin: rightPadding
                                bottomMargin: bottomPadding
                            }
                        }
                    }
                }
                bottomMargin: height
                onMovementEnded: {
                    //close
                    if ((root.height + mainFlickable.contentY) < root.height/2) {
                        closeAnimation.to = -root.height;
                        closeAnimation.running = true;
                    } else if ((root.height*0.6 + mainFlickable.contentY) > flickableContents.height) {
                        closeAnimation.to = flickableContents.height
                        closeAnimation.running = true;

                    //reset to the default opened position
                    } else if (mainFlickable.contentY < openAnimation.topOpenPosition) {
                        openAnimation.from = mainFlickable.contentY;
                        openAnimation.to = openAnimation.topOpenPosition;
                        openAnimation.running = true;
                    //reset to the default "bottom" opened position
                    } else if (mainFlickable.contentY > openAnimation.bottomOpenPosition) {
                        openAnimation.from = mainFlickable.contentY;
                        openAnimation.to = openAnimation.bottomOpenPosition;
                        openAnimation.running = true;
                    }
                }
                onFlickEnded: movementEnded();
            }
        }
    }
}
