// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.7

import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0

import QtQuick.Window 2.2 as Window
import QtQuick.Layouts 1.1

RowLayout {
    visible: step2
    anchors {
        left: parent.left
        verticalCenter: parent.verticalCenter
        right: parent.right
    }

    TutorialPane {
        anchors{
            right: arrow.left
            verticalCenter: arrow.verticalCenter
        }

        contentWidth: (parent.width - arrow.width) * 0.75
        title: xi18n("Context Drawer")
        description: xi18n("By swiping from right to left you can access context drawer with functions related to Sky Map. This menu is available only on Sky Map.")

        onNextClicked: {
            step2 = false
            step3 = true
        }
    }

    Image {
        id: arrow
        rotation: 180
        source: "../../images/tutorial-arrow-horizontal.png"
    }
}
