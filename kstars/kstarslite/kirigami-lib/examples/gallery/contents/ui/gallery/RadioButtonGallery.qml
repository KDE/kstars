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

    title: "Radio buttons"
    actions {
        main: Action {
            iconName: "document-edit"
            text: "Main Action Text"
            onTriggered: {
                showPassiveNotification("Action button in buttons page clicked");
            }
        }
        left: Action {
            iconName: "folder-sync"
            text: "Left Action Text"
            onTriggered: {
                showPassiveNotification("Left action triggered")
            }
        }
    }


    ColumnLayout {
        width: page.width
        Controls.ExclusiveGroup {
            id: radioGroup
        }
        Controls.ExclusiveGroup {
            id: radioGroup2
        }

        Item {
            Layout.fillWidth: true
            Layout.minimumHeight: units.gridUnit * 10
            GridLayout {
                anchors.centerIn: parent
                columns: 3
                rows: 3
                rowSpacing: Units.smallSpacing

                Item {
                    width: 1
                    height: 1
                }
                Label {
                    text: "Normal"
                }
                Label {
                    text: "Disabled"
                    enabled: false
                }
                Label {
                    text: "On"
                }
                Controls.RadioButton {
                    text: "On"
                    checked: true
                    exclusiveGroup: radioGroup
                }
                Controls.RadioButton {
                    text: "On"
                    checked: true
                    enabled: false
                    exclusiveGroup: radioGroup2
                }
                Label {
                    text: "Off"
                }
                Controls.RadioButton {
                    text: "Off"
                    checked: false
                    exclusiveGroup: radioGroup
                }
                Controls.RadioButton {
                    text: "Off"
                    checked: false
                    enabled: false
                    exclusiveGroup: radioGroup2
                }
            }
        }
    }
}
