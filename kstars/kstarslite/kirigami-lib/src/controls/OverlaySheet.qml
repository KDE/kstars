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
import org.kde.kirigami 1.0
import "private"
import "templates" as T

/**
 * An overlay sheet that covers the current Page content.
 * Its contents can be scrolled up or down, scrolling all the way up or
 * all the way down, dismisses it.
 * Use this for big, modal dialogs or information display, that can't be
 * logically done as a new separate Page, even if potentially
 * are taller than the screen space.
 */
T.OverlaySheet {
    id: root

    background: Item {
        anchors.fill: parent
        //Why not a shadow or rectangularglow?
        //on some android devices they break badly when the OverlaySheet is bigger than
        //the screen
        CornerShadow {
            corner: Qt.BottomRightCorner
            anchors {
                right: parent.left
                bottom: parent.top
            }
        }
        CornerShadow {
            corner: Qt.BottomLeftCorner
            anchors {
                left: parent.right
                bottom: parent.top
            }
        }
        CornerShadow {
            corner: Qt.TopRightCorner
            anchors {
                right: parent.left
                top: parent.bottom
            }
        }
        CornerShadow {
            corner: Qt.TopLeftCorner
            anchors {
                left: parent.right
                top: parent.bottom
            }
        }
        EdgeShadow {
            edge: Qt.BottomEdge
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.top
            }
        }
        EdgeShadow {
            edge: Qt.TopEdge
            anchors {
                left: parent.left
                right: parent.right
                top: parent.bottom
            }
        }
        EdgeShadow {
            edge: Qt.LeftEdge
            anchors {
                top: parent.top
                bottom: parent.bottom
                left: parent.right
            }
        }
        EdgeShadow {
            edge: Qt.RightEdge
            anchors {
                top: parent.top
                bottom: parent.bottom
                right: parent.left
            }
        }
        Rectangle {
            anchors.fill: parent
            color: Theme.viewBackgroundColor
        }
    }
}
