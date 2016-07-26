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

import QtQuick 2.0
import QtQuick.Controls 1.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 1.0

ScrollablePage {
    id: page
    Layout.fillWidth: true
    title: "Long List view"

    supportsRefreshing: true
    onRefreshingChanged: {
        if (refreshing) {
            refreshRequestTimer.running = true;
        } else {
            showPassiveNotification("Example refreshing completed")
        }
    }

    background: Rectangle {
        color: Theme.viewBackgroundColor
    }

    ListView {
        Timer {
            id: refreshRequestTimer
            interval: 3000
            onTriggered: page.refreshing = false
        }
        model: 200
        delegate: BasicListItem {
            label: "Item " + modelData
        }
    }
}
