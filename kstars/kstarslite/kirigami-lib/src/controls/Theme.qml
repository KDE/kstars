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

import QtQuick 2.4

pragma Singleton


QtObject {
    id: theme

    property color textColor: "#31363b"
    property color disabledTextColor: "#9931363b"

    property color highlightColor: "#2196F3"
    property color highlightedTextColor: "#eff0fa"
    property color backgroundColor: "#eff0f1"
    property color linkColor: "#2196F3"
    property color visitedLinkColor: "#2196F3"

    property color buttonTextColor: "#31363b"
    property color buttonBackgroundColor: "#eff0f1"
    property color buttonHoverColor: "#2196F3"
    property color buttonFocusColor: "#2196F3"

    property color viewTextColor: "#31363b"
    property color viewBackgroundColor: "#fcfcfc"
    property color viewHoverColor: "#2196F3"
    property color viewFocusColor: "#2196F3"

    property font defaultFont: fontMetrics.font

    property list<QtObject> children: [
        TextMetrics {
            id: fontMetrics
        }
    ]
}
