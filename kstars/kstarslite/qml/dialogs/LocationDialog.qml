// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
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
    contentItem: locationColumn
    title: xi18n("Set Geolocation")

    function filterCities() {
        LocationDialogLite.filterCity(cityFilter.text, provinceFilter.text, countryFilter.text)
    }

    onVisibleChanged: {
        filterCities()
    }

    ColumnLayout {
        id: locationColumn
        spacing: 5 * num.dp
        anchors{
            bottom: parent.bottom
            bottomMargin: 15 * num.dp
        }

        Flow {
            anchors {
                left: parent.left
                right: parent.right
            }

            KSLabel {
                text: xi18n("Current Location: ")
            }

            KSLabel {
                text: LocationDialogLite.currentLocation
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "grey"
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            flow: window.isPortrait ? GridLayout.TopToBottom : GridLayout.LeftToRight

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                KSLabel {
                    text: xi18n("City filter: ")
                }
                KSTextField {
                    id: cityFilter
                    Layout.fillWidth: true
                    onTextChanged: {
                        filterCities()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                KSLabel {
                    text: xi18n("Province filter: ")
                }

                KSTextField {
                    id: provinceFilter
                    Layout.fillWidth: true
                    onTextChanged: {
                        filterCities()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                KSLabel {
                    text: xi18n("Country filter: ")
                }
                KSTextField {
                    id: countryFilter
                    Layout.fillWidth: true
                    onTextChanged: {
                        filterCities()
                    }
                }
            }
        }

        KSListView {
            model: CitiesModel
            textRole: "display"

            Layout.fillWidth: true
            Layout.fillHeight: true

            checkCurrent: true
            currentIndex: LocationDialogLite.currLocIndex
            onClickCheck: false

            onClicked: {
                locationsGeoMenu.openMenu(text)
            }
        }

        Button {
            anchors {
                bottom: parent.bottom
            }

            text: "Add Location"
            onClicked: {
                locationEdit.openAdd()
            }
        }

        Button {
            anchors {
                bottom: parent.bottom
                right: parent.right
            }

            text: "Set from GPS"
            onClicked: {
                locationEdit.setAutomaticallyFromGPS()
            }
        }
    }
}
