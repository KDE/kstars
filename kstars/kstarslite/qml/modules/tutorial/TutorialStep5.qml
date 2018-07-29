// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

import QtQuick 2.7

import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Controls.Universal 2.0

import QtQuick.Window 2.2 as Window
import QtQuick.Layouts 1.1

TutorialPane {
    visible: step5
    anchors.centerIn: parent

    contentWidth: parent.width * 0.75
    title: xi18n("Set Location")
    description: xi18n("Congratulations with your first steps in KStars Lite. Your tutorial is almost
    over. The last step to do is to set your location (you can do that either manually or from GPS).
    Click next to proceed.")

    onNextClicked: {
        step5 = false
        stackView.push(locationDialog)
        //Finish tutorial
        exitTutorial()
    }
}
