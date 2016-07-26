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

import QtQuick 2.0
import QtGraphicalEffects 1.0
import org.kde.kirigami 1.0
import org.kde.kquickcontrolsaddons 2.0

Item {
    id: root
    property alias source: icon.icon
    property alias smooth: icon.smooth
    property bool active: false
    property bool valid: true
    property bool selected: false
    onSelectedChanged: {
        if (selected) {
            icon.state = QIconItem.SelectedState;
            active = false;
        } else if (icon.state == QIconItem.SelectedState) {
            icon.state = QIconItem.DefaultState;
        }
    }
    onActiveChanged: {
        if (active) {
            icon.state = QIconItem.ActiveState;
            selected = false;
        } else if (icon.state == QIconItem.ActiveState) {
            icon.state = QIconItem.DefaultState;
        }
    }

    implicitWidth: icon.icon != "" ? Units.iconSizes.smallMedium : 0
    implicitHeight: icon.icon != "" ? Units.iconSizes.smallMedium : 0

    QIconItem {
        id: icon
        anchors.fill: parent
    }
}
