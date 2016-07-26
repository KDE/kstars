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
import QtQuick.Controls 1.0 as Controls
import QtQuick.Controls.Private 1.0

Rectangle {
    color: listItem.checked || (listItem.pressed && !listItem.checked && !listItem.sectionDelegate) ? Theme.highlightColor : Theme.viewBackgroundColor

    visible: listItem.ListView.view ? listItem.ListView.view.highlight === null : true
    Rectangle {
        anchors.fill: parent
        visible: !Settings.isMobile
        color: Theme.highlightColor
        opacity: listItem.containsMouse && !listItem.pressed ? 0.2 : 0
        Behavior on opacity { NumberAnimation { duration: Units.longDuration } }
    }
    Behavior on color {
        ColorAnimation { duration: Units.longDuration }
    }

    Rectangle {
        id: separator
        color: Theme.textColor
        opacity: 0.2
        visible: listItem.separatorVisible
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: Math.ceil(Units.smallSpacing / 5);
    }
}

