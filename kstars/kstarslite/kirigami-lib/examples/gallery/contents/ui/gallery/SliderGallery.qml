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

    title: "Sliders"

    actions {
        left: Action {
            iconName: "folder-sync"
            text: "Left Action Text"
            onTriggered: {
                showPassiveNotification("Left action triggered")
            }
        }
        right: Action {
            iconName: "configure"
            text: "Right Action Text"
            onTriggered: {
                showPassiveNotification("Right action triggered")
            }
        }
    }

    ColumnLayout {
        width: page.width

        Item {
            Layout.fillWidth: true
            Layout.minimumHeight: Units.gridUnit * 20
            ColumnLayout {
                anchors.centerIn: parent
                spacing: Units.smallSpacing

                Label {
                    text: "Normal:"
                }
                Controls.Slider {
                    Layout.minimumWidth: Units.gridUnit * 15
                    value: 2
                    maximumValue: 5.0
                }
                Label {
                    text: "Disabled:"
                }
                Controls.Slider {
                    enabled: false
                    Layout.minimumWidth: Units.gridUnit * 15
                    value: 2
                    maximumValue: 5.0
                }
                Label {
                    text: "Thickmarks:"
                }
                Controls.Slider {
                    Layout.minimumWidth: Units.gridUnit * 15
                    tickmarksEnabled: true
                    maximumValue: 5.0
                    stepSize: 1.0
                    value: 3
                }
                Label {
                    text: "Vertical:"
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Controls.Slider {
                        Layout.minimumWidth: 2
                        Layout.minimumHeight: Units.gridUnit * 10
                        value: 2
                        maximumValue: 5.0
                        orientation: Qt.Vertical
                    }
                    Controls.Slider {
                        Layout.minimumWidth: 2
                        Layout.minimumHeight: Units.gridUnit * 10
                        value: 3
                        tickmarksEnabled: true
                        maximumValue: 5.0
                        stepSize: 1.0
                        orientation: Qt.Vertical
                    }
                }
            }
        }
    }
}

