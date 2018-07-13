// Copyright (C) 2018 Csaba Kertesz <csaba.kertesz@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

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
