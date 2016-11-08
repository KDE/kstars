// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

import QtQuick.Controls 2.0
import QtQuick 2.6
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../../modules"
import QtQuick.Controls.Material 2.0

Popup {
    //Change it to Popup when it will become more stable
    id: colorSPopup
    focus: true
    modal: true
    height: parent.height > colorsList.implicitHeight ? colorsList.implicitHeight : parent.height
    signal colorSchemeChanged()

    property string currentCScheme: colorsModel.get(colorsList.currentIndex).scheme

    onCurrentCSchemeChanged: {
        updateTheme(currentCScheme)
    }

    function updateTheme(colorScheme) {
        if ( Qt.platform.os == "android" )  {
            if ( colorScheme == "cs_night") {
                window.Material.theme = Material.Dark //On Android we want to have dark theme only for cs_night
            } else {
                window.Material.theme = Material.Light //Light theme for all other color schemes as on Android they are light
            }
        } else {
            window.Material.theme = Material.Dark //Dark theme is suitable for all color themes
        }
    }

    function formatColorScheme(schemeName) {
        return schemeName.substring(3) + ".colors"
    }

    background: Rectangle {
        anchors.fill: parent
        color: num.sysPalette.base
    }

    KSListView {
        id: colorsList
        anchors.centerIn: parent
        checkCurrent: true

        model: ListModel {
            id: colorsModel

            Component.onCompleted: {
                append({ name: xi18n("Classic"), scheme: "cs_classic" });
                append({ name: xi18n("Star Chart"), scheme: "cs_chart" });
                append({ name: xi18n("Night Vision"), scheme: "cs_night" });
                append({ name: xi18n("Moonless Night"), scheme: "cs_moonless-night" });
            }
        }

        Connections {
            target: KStarsLite
            onDataLoadFinished: {
                //Set current index to current scheme color
                var colorScheme = KStarsData.colorSchemeName()
                for(var i = 0; i < colorsList.model.count; ++i) {
                    if(formatColorScheme(colorsList.model.get(i).scheme) == colorScheme) {
                        colorsList.currentIndex = i
                        //KStarsLite.loadColorScheme(colorScheme)
                    }
                }
            }
        }

        onClicked: {
            var item = colorsModel.get(colorsList.currentIndex)

            KStarsLite.loadColorScheme(formatColorScheme(item.scheme));
            notification.showNotification("Set color scheme to " + item.name)

            colorSchemeChanged()
            close()
        }

        textRole: "name"
    }
}
