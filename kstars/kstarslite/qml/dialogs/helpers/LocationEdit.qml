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

        onSourceErrorChanged: {
            if (sourceError == PositionSource.NoError)
                return

            var errorDesc = ""

            if (sourceError == 2 || sourceError == 1) {
                errorDesc = xi18n("No location service (GPS, Wi-Fi etc.) is available. Please, switch on location service and retry")
            } else if (sourceError == 4) {
                errorDesc = xi18n("Unknown error occured. Please contact application developer.")
            }

            notification.showNotification("Location error: " + errorDesc)
            active = false
            sourceError = positionSource.NoError
        }

        onUpdateTimeout: {
            notification.showNotification(xi18n("Timeout occured. Try again."))
        }

        onActiveChanged: {
            if(positionSource.active) {
                locationLoading.open()
            } else {
                locationLoading.close()
            }
        }

        onPositionChanged: {
            if( (!positionSource.position.latitudeValid ||
                 !positionSource.position.longitudeValid) ) {
                latField.text = ""
                longField.text = ""
                if(isLoaded) {
                    notification.showNotification(xi18n("Couldn't get latitude and longitude from GPS"))
                }
            } else {
                var lat = positionSource.position.coordinate.latitude
                var longitude = positionSource.position.coordinate.longitude
                latField.text = lat
                longField.text = longitude
                notification.showNotification(xi18("Found your longitude and altitude"))
            }

            locationLoading.close()
        }

        preferredPositioningMethods: PositionSource.AllPositioningMethods
    }

    /**
      closes the popup and clears all text fields
    */

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
                    Label {
                        text: xi18n("City: ")
                    }

                    TextField {
                        id: cityField
                        Layout.fillWidth: true
                        onTextChanged: fieldsChanged = true
                        readOnly: isReadOnly
                    }
                }

                RowLayout {
                    Label {
                        text: xi18n("Province: ")
                    }

                    TextField {
                        id: provinceField
                        Layout.fillWidth: true
                        onTextChanged: fieldsChanged = true
                        readOnly: isReadOnly
                    }
                }

                RowLayout {
                    Label {
                        text: xi18n("Country: ")
                    }

                    TextField {
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
                    Label {
                        text: xi18n("Latitude: ")
                    }

                    TextField {
                        id: latField
                        Layout.fillWidth: true
                        readOnly: isReadOnly
                    }
                }

                RowLayout {

                    Label {
                        text: xi18n("Longitude: ")
                    }

                    TextField {
                        id: longField
                        Layout.fillWidth: true
                        readOnly: isReadOnly
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 10

                    RowLayout {
                        Label {
                            text: xi18n("UT offset: ")
                        }

                        ComboBox {
                            id: comboBoxTZ
                            model: LocationDialogLite.TZList
                        }
                    }

                    RowLayout {
                        Label {
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
                    positionSource.update()
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
