// SPDX-FileCopyrightText: 2018 Csaba Kertesz <csaba.kertesz@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0
import "../constants" 1.0
import "../modules"

KSPage {
    id: aboutDialog
    objectName: "aboutDialog"
    title: xi18n("About")

    property alias versionText: versionLabel.text
    property alias buildText: buildLabel.text
    property alias teamText: teamLabel.text
    property alias licenseText: licenseLabel.text

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true

        KSLabel {
            text: xi18n("KStars")
        }

        KSLabel {
            id: versionLabel
            text: ""
        }

        KSLabel {
            text: xi18n("Desktop Planetarium")
        }

        KSLabel {
            id: buildLabel
            text: ""
        }

        KSLabel {
            id: teamLabel
            text: ""
        }

        KSLabel {
            textFormat: Text.RichText
            text: "<a href=\"https://edu.kde.org/kstars\">https://edu.kde.org/kstars</a>"
        }

        KSLabel {
            id: licenseLabel
            text: ""
        }
    }
}
