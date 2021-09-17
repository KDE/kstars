// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.7

import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0

import QtQuick.Window 2.2 as Window
import QtQuick.Layouts 1.1

import "../../constants" 1.0
import "../../modules"

Popup {
    id: studyMode
    contentWidth: parent.width * 0.75

    focus: true
    modal: true

    background: Rectangle {
        color: Num.sysPalette.base
    }

    Column {
        id: studyCol
        width: parent.width
        height: childrenRect.height

        KSLabel {
            width: parent.width
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: xi18n("Welcome to KStars Lite")
            font.pointSize: 20
        }

        KSText {
            width: parent.width
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: xi18n("KStars Lite is a free, open source, cross-platform Astronomy Software designed for mobile devices.")
        }

        KSText {
            width: parent.width
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: xi18n("A quick tutorial will introduce you to main functions of KStars Lite")
        }

        Flow {
            property int buttonWidth: children[0].width + children[1].width + spacing * 2
            width: parent.width > buttonWidth ? buttonWidth : parent.width
            spacing: 5
            anchors.horizontalCenter: parent.horizontalCenter

            Button {
                text: xi18n("Close")
                onClicked: askExitTutorial()
            }

            Button {
                text: xi18n("Start tutorial")
                onClicked: {
                    studyMode.close()
                    step1 = true
                }
            }
        }
    }
}
