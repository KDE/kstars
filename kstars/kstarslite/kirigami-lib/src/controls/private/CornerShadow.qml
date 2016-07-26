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
import QtGraphicalEffects 1.0
import org.kde.kirigami 1.0

RadialGradient {
    id: shadow
    /**
     * corner: enumeration
     * This property holds the corner of the shadow that will determine
     * the direction of the gradient.
     * The acceptable values are:
     * Qt.TopLeftCorner, TopRightCorner, BottomLeftCorner, BottomRightCorner
     */
    property int corner: Qt.TopRightCorner

    width: Units.gridUnit/2
    height: Units.gridUnit/2

    horizontalOffset: {
        switch (corner) {
            case Qt.TopLeftCorner:
            case Qt.BottomLeftCorner:
                return -width/2; 
            default:
                return width/2;
        }
    }
    verticalOffset: {
        switch (corner) {
            case Qt.TopLeftCorner:
            case Qt.TopRightCorner:
                return -width/2; 
            default:
                return width/2;
        }
    }

    gradient: Gradient {
        GradientStop {
            position: 0.0
            color: Qt.rgba(0, 0, 0, 0.2)
        }
        GradientStop {
            position: 0.3
            color: Qt.rgba(0, 0, 0, 0.1)
        }
        GradientStop {
            position: 1.0
            color:  "transparent"
        }
    }
}

