/*
 *   Copyright 2016 Marco Martin <mart@kde.org>
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

import "../../templates" as T

/**
 * Overlay Drawers are used to expose additional UI elements needed for
 * small secondary tasks for which the main UI elements are not needed.
 * For example in Okular Active, an Overlay Drawer is used to display
 * thumbnails of all pages within a document along with a search field.
 * This is used for the distinct task of navigating to another page.
 *
 */
T.OverlayDrawer {
    id: root

//BEGIN Properties
    background: Rectangle {
        color: Theme.viewBackgroundColor
        property Item handleBackground: Item {
            
        }
        
        Item {
            id: drawerHandle
            z: -1

            anchors {
                right: root.edge == Qt.LeftEdge ? undefined : parent.left
                left: root.edge == Qt.RightEdge ? undefined : parent.right
                bottom: parent.bottom
            }
            visible: root.enabled && (root.edge == Qt.LeftEdge || root.edge == Qt.RightEdge)
            width: Units.iconSizes.medium
            height: width
            opacity: root.handleVisible ? 1 : 0
            Behavior on opacity {
                NumberAnimation {
                    duration: Units.longDuration
                    easing.type: Easing.InOutQuad
                }
            }
            transform: Translate {
                id: translateTransform
                x: root.handleVisible ? 0 : (root.edge == Qt.LeftEdge ? -drawerHandle.width : drawerHandle.width)
                Behavior on x {
                    NumberAnimation {
                        duration: Units.longDuration
                        easing.type: !root.handleVisible ? Easing.OutQuad : Easing.InQuad
                    }
                }
            }
            Rectangle {
                id: handleGraphics
                color: Theme.viewBackgroundColor
                opacity: 0.3 + root.position
                anchors.fill: parent
            }

            Loader {
                anchors.centerIn: handleGraphics
                width: height
                height: Units.iconSizes.smallMedium - Units.smallSpacing * 2
                source: root.edge == Qt.LeftEdge ? Qt.resolvedUrl("../../templates/private/MenuIcon.qml") : (root.edge == Qt.RightEdge ? Qt.resolvedUrl("../../templates/private/ContextIcon.qml") : "")
                onItemChanged: {
                    if(item) {
                        item.morph = Qt.binding(function(){return root.position})
                    }
                }
            }
            Rectangle {
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                }
                color: Theme.textColor
                opacity: 0.3
                height: Math.ceil(Units.smallSpacing / 5)
            }
            Rectangle {
                anchors {
                    left: root.edge == Qt.LeftEdge ? parent.right : undefined
                    right: root.edge == Qt.RightEdge ? parent.left : undefined
                    top: parent.top
                    bottom: parent.bottom
                }
                color: Theme.textColor
                opacity: 0.3
                width: Math.ceil(Units.smallSpacing / 5)
            }
        }
        
        Rectangle {
            z: -2
            anchors {
                left: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            color: Theme.textColor
            opacity: root.position == 0 ? 0 : 0.3
            width: Math.ceil(Units.smallSpacing / 5)
        }
    }

    //default to a sidebar in desktop mode
    modal: edge == Qt.TopEdge || edge == Qt.BottomEdge
    opened: true
}
