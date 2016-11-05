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
import QtQuick 2.7
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../../modules"
import QtPositioning 5.2

KSPage {
    id: locationEdit
    title: editMode ? xi18n("Edit location") : (isReadOnly ? xi18n("View location") : xi18n("Add location"))
    property bool editMode: false
    property bool isAvailable: positionSource.valid
    property bool isReadOnly: false
    property string geoName
    property bool fieldsChanged: false //true whenever either city, province or country fields are changed. Turned to false every time this page is opened
    property string loadingText //Text used in location loading popup
    property string fetchingCoordinatesLoading: xi18n("Please, wait while we are fetching coordinates")

    property bool fetchingName: false // true when we are fetchingN name of location
    property bool addAutomatically: false //true if user wants add location automatically without manually editing the fields

    signal locationFetched(var _lat, var _lng) //emitted when location is fetched in auto mode
    signal locNameFetched(var _city, var _region, var _country) //emitted when location nane is fetched or was failed to fetch in auto mode

    /*This function sets coordinates from GPS automatically, without asking user to fill information
    about location */
    function setAutomaticallyFromGPS() {
        addAutomatically = true
        positionSource.stop()
        positionSource.start()
        loadingText = fetchingCoordinatesLoading
        if(!positionSource.valid) {
            positionSource.stop()
            notification.showNotification(xi18("Positioning is not available on your device"))
        }
    }

    property double lat
    property double lng
    property string city
    property string region
    property string country
    property int tz

    onLocationFetched: {
        lat = _lat
        lng = _lng
    }

    Timer {
        id: nameFetchTimeout
        interval: 20000;
        onTriggered: {
            locationLoading.close()
            var city = xi18n("Default city")
            var province = xi18n("Default province")
            var country = xi18n("Default country")
            if(addAutomatically) {
                notification.showNotification(xi18n("Couldn't fetch location name (check your internet connection). Added with default name"))
                if(!LocationDialogLite.addCity(city, province, country,
                                               lat, lng, tz,
                                               "--")) {
                    notification.showNotification(xi18n("Failed to set location"))
                    return
                }

                if(LocationDialogLite.setLocation(city + ", " + province + ", " + country)) {
                    notification.showNotification(xi18n("Successfully set your location"))
                } else {
                    notification.showNotification(xi18n("Couldn't set your location"))
                }
            } else {
                notification.showNotification(xi18n("Couldn't fetch location name (check your internet connection). Set default name"))
                cityField.text = city
                provinceField.text = province
                countryField.text = country
                comboBoxTZ.currentIndex = comboBoxTZ.find(tz)
            }
            fetchingName = false
            addAutomatically = false
        }
    }

    onLocNameFetched: {
        nameFetchTimeout.running = false
        city = _city
        region = _region
        country = _country

        if(!LocationDialogLite.addCity(city, region, country,
                                       lat, lng, tz,
                                       "--")) {
            notification.showNotification(xi18n("Failed to set location"))
        }

        if(LocationDialogLite.setLocation(city + ", " + region + ", " + country)) {
            notification.showNotification(xi18n("Successfully set your location"))
        } else {
            notification.showNotification(xi18n("Couldn't set your location"))
        }

        addAutomatically = false
    }

    function openAdd() {
        editMode = false
        isReadOnly = false
        stackView.push(this)
        fieldsChanged = false
    }

    function openEdit(fullName, readOnly) {
        geoName = fullName
        isReadOnly = readOnly

        if(!readOnly) {
            editMode = true
        }

        cityField.text = LocationDialogLite.getCity(fullName)
        provinceField.text = LocationDialogLite.getProvince(fullName)
        countryField.text =  LocationDialogLite.getCountry(fullName)

        latField.text = LocationDialogLite.getLatitude(fullName)
        longField.text = LocationDialogLite.getLongitude(fullName)

        comboBoxTZ.currentIndex = LocationDialogLite.getTZ(fullName)
        comboBoxDST.currentIndex = LocationDialogLite.getDST(fullName)
        stackView.push(this)
        fieldsChanged = false
    }

    PositionSource {
        id: positionSource

        property bool error: false

        onSourceErrorChanged: {
            if (sourceError == PositionSource.NoError)
                return

            var errorDesc = ""

            if (sourceError == 2 || sourceError == 1) {
                errorDesc = xi18n("No location service (GPS, cellular service, etc.) is available.\nPlease, switch on location service and retry")
            } else if (sourceError == 4) {
                errorDesc = xi18n("Unknown error occurred. Please contact application developer.")
            }

            notification.showNotification(errorDesc)
            positionSource.stop()
            error = true
            locationLoading.close()
        }

        onUpdateTimeout: {
            notification.showNotification(xi18n("Timeout occurred. Try again."))
            locationLoading.close()
        }

        onActiveChanged: {
            if(positionSource.active && !error) {
                locationLoading.open()
            } else if (!fetchingName) {
                locationLoading.close()
                error = false
            }
        }

        onPositionChanged: {
            if(isLoaded) {
                notification.showNotification(xi18n("Found your longitude and altitude"))
                var lat = positionSource.position.coordinate.latitude
                var lng = positionSource.position.coordinate.longitude
                latField.text = lat
                longField.text = lng
                if(addAutomatically) {
                    locationFetched(lat, lng)
                }

                tz = (new Date().getTimezoneOffset()/60)*-1
                loadingText = xi18n("Please, wait while we are retrieving location name")
                fetchingName = true // must be set to true before we are stopping positioning service
                positionSource.stop()
                LocationDialogLite.getNameFromCoordinates(lat, lng)
                nameFetchTimeout.running = true
                setTZComboBox(new Date().getTimezoneOffset())
            }
        }
        preferredPositioningMethods: PositionSource.AllPositioningMethods
    }

    function setTZComboBox(TZMinutes) {
        var TZ = (TZMinutes/60)*-1
        comboBoxTZ.currentIndex = comboBoxTZ.find(TZ)
    }

    Connections {
        target: LocationDialogLite
        onNewNameFromCoordinates: {
            if(addAutomatically) {
                locNameFetched(city, region, country)
            }
            cityField.text = city
            provinceField.text = region
            countryField.text = country
            fetchingName = false
            locationLoading.close()
            addAutomatically = false
        }
    }

    //close the popup and clears all text fields
    onVisibleChanged: {
        if(!visible) {
            cityField.clear()
            provinceField.clear()
            countryField.clear()
            latField.clear()
            longField.clear()
        }
    }

    ColumnLayout {
        anchors {
            left: parent.left
            right: parent.right
        }

        GridLayout {
            anchors {
                left: parent.left
                right: parent.right
            }

            flow: window.isPortrait ? GridLayout.TopToBottom : GridLayout.LeftToRight

            ColumnLayout {
                anchors.top: parent.top
                Layout.maximumWidth: window.isPortrait ? parent.width : parent.width/2

                RowLayout {
                    KSLabel {
                        text: xi18n("City: ")
                    }

                    KSTextField {
                        id: cityField
                        Layout.fillWidth: true
                        onTextChanged: fieldsChanged = true
                        readOnly: isReadOnly
                    }
                }

                RowLayout {
                    KSLabel {
                        text: xi18n("Province: ")
                    }

                    KSTextField {
                        id: provinceField
                        Layout.fillWidth: true
                        onTextChanged: fieldsChanged = true
                        readOnly: isReadOnly
                    }
                }

                RowLayout {
                    KSLabel {
                        text: xi18n("Country: ")
                    }

                    KSTextField {
                        id: countryField
                        Layout.fillWidth: true
                        onTextChanged: fieldsChanged = true
                        readOnly: isReadOnly
                    }
                }
            }

            Item {
                height: window.isPortrait ? 15 : 0
            }

            ColumnLayout {
                Layout.maximumWidth: window.isPortrait ? parent.width : parent.width/2

                RowLayout {
                    KSLabel {
                        text: xi18n("Latitude: ")
                    }

                    KSTextField {
                        id: latField
                        Layout.fillWidth: true
                        readOnly: isReadOnly
                    }
                }

                RowLayout {

                    KSLabel {
                        text: xi18n("Longitude: ")
                    }

                    KSTextField {
                        id: longField
                        Layout.fillWidth: true
                        readOnly: isReadOnly
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 10

                    RowLayout {
                        KSLabel {
                            text: xi18n("UT offset: ")
                        }

                        ComboBox {
                            id: comboBoxTZ
                            model: LocationDialogLite.TZList
                        }
                    }

                    RowLayout {
                        KSLabel {
                            text: xi18n("DST Rule: ")
                        }

                        ComboBox {
                            id: comboBoxDST
                            model: LocationDialogLite.DSTRules
                        }
                    }
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 10

            Button {
                visible: !isReadOnly
                text: "Set from GPS"
                enabled: isAvailable
                onClicked: {
                    positionSource.stop()
                    positionSource.start()
                    loadingText = fetchingCoordinatesLoading
                    if(!positionSource.valid) {
                        positionSource.stop()
                        notification.showNotification(xi18("Positioning is not available on your device"))
                    }
                }

                Connections {
                    target: locationLoading
                    onClosed: {
                        positionSource.stop()
                    }
                }
            }

            Button {
                //enabled:
                visible: !isReadOnly
                text: editMode ? xi18n("Save") : xi18n("Add")
                onClicked: {
                    if(cityField.text == "") {
                        notification.showNotification(xi18n("Please, fill in city"))
                        return
                    } else if(countryField.text == "") {
                        notification.showNotification(xi18n("Please, fill in country"))
                        return
                    } else if(latField.text == "") {
                        notification.showNotification(xi18n("Please, fill in latitude"))
                        return
                    } else if(longField.text == "") {
                        notification.showNotification(xi18n("Please, fill in longitude"))
                        return
                    }

                    if(!LocationDialogLite.checkLongLat(longField.text, latField.text)) {
                        notification.showNotification(xi18n("Either longitude or latitude values are not valid"))
                        return
                    }

                    if(fieldsChanged) {
                        if(LocationDialogLite.isDuplicate(cityField.text, provinceField.text, countryField.text)) {
                            notification.showNotification(xi18n("This location already exists. Change either city, province or country"))
                            return
                        }
                    }

                    //Fullname of new location
                    var fullName = cityField.text + ", "
                    if(provinceField.text != "") {
                        fullName += provinceField.text + ", "
                    }
                    fullName += countryField.text

                    if(!editMode) {
                        if(!LocationDialogLite.addCity(cityField.text, provinceField.text, countryField.text,
                                                       latField.text, longField.text, comboBoxTZ.currentText,
                                                       comboBoxDST.currentText)) {
                            notification.showNotification(xi18n("Failed to add location"))
                            return
                        } else {
                            notification.showNotification(xi18n("Added new location - " + fullName))
                        }
                    } else {
                        if(!LocationDialogLite.editCity(geoName, cityField.text, provinceField.text, countryField.text,
                                                        latField.text, longField.text, comboBoxTZ.currentText,
                                                        comboBoxDST.currentText)) {
                            notification.showNotification(xi18n("Failed to edit city"))
                            return
                        }
                    }

                    locationDialog.filterCities()
                    if(!editMode) {
                        //If we are adding location then open menu with newly added location
                        locationsGeoMenu.openMenu(fullName)
                    }

                    stackView.pop()
                }
            }

            Button {
                text: xi18n("Cancel")
                onClicked: {
                    stackView.pop()
                }
            }
        }
    }
}
