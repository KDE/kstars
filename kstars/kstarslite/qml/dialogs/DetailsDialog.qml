// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.7
import QtQuick.Window 2.2
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import "helpers"
import "menus"
import "../modules"
import "../constants" 1.0

KSPage {
    title: SkyMapLite.clickedObjectLite.translatedName + " - " + tabBar.currentItem.text
    Item {
        anchors.fill: parent

        TabBar {
            id: tabBar
            property bool isTab: true
            spacing: 20
            currentIndex: detailsSwipeView.currentIndex
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }
            background: Rectangle {
                anchors.fill: parent
                color: Num.sysPalette.base
            }

            KSTabButton {
                text: xi18n("General")
            }

            KSTabButton {
                text: xi18n("Position")
            }

            KSTabButton {
                Component.onCompleted: {
                    var oldParent = parent
                    parent = Qt.binding(function() { return DetailDialogLite.isLinksOn ? oldParent : null })
                }

                text: xi18n("Links")
            }

            KSTabButton {
                Component.onCompleted: {
                    var oldParent = parent
                    parent = Qt.binding(function() { return DetailDialogLite.isLogOn ? oldParent : null })
                }
                text: xi18n("Log")
            }
        }

        SwipeView {
            id: detailsSwipeView
            anchors {
                top:tabBar.bottom
                topMargin: 10
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }

            currentIndex: tabBar.currentIndex
            clip: true

            Pane {
                clip: true

                background: Rectangle {
                    anchors.fill: parent
                    color: Num.sysPalette.base
                }

                Flickable {
                    anchors.fill: parent
                    ScrollBar.vertical: ScrollBar { }
                    flickableDirection: Flickable.VerticalFlick

                    contentHeight: generalCol.height

                    Column {
                        id: generalCol
                        width: parent.width
                        spacing: 15

                        KSText {
                            text: DetailDialogLite.name
                            font.pointSize: 16
                            width: parent.width
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Image {
                            source: DetailDialogLite.thumbnail
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        KSText {
                            text: DetailDialogLite.typeInConstellation
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pointSize: 12
                        }

                        Column {
                            id: telescopesCol
                            width: parent.width
                            property var telescopeControls: []

                            Connections {
                                target: ClientManagerLite
                                onTelescopeAdded: {
                                    var controls = Qt.createComponent("helpers/TelescopeControl.qml")
                                    var controlsObj = controls.createObject(telescopesCol)
                                    controlsObj.telescope = newTelescope
                                    telescopesCol.telescopeControls.push(controlsObj)

                                }

                                onTelescopeRemoved: {
                                    for(var i = 0; i < telescopesCol.telescopeControls.length; ++i) {
                                        if(telescopesCol.telescopeControls[i].telescope == delTelescope) {
                                            telescopesCol.telescopeControls[i].parent = null
                                            telescopesCol.telescopeControls[i].destroy()
                                        }
                                    }
                                }
                            }
                        }

                        DetailsItem {
                            label: xi18n("Magnitude")
                            value: DetailDialogLite.magnitude
                        }

                        DetailsItem {
                            label: xi18n("Distance")
                            value: DetailDialogLite.distance
                        }

                        DetailsItem {
                            label: xi18n("B - V Index")
                            value: DetailDialogLite.BVindex
                        }

                        DetailsItem {
                            label: xi18n("Size")
                            value: DetailDialogLite.angSize
                        }

                        DetailsItem {
                            label: xi18n("Illumination")
                            value: DetailDialogLite.illumination
                        }

                        DetailsItem {
                            label: xi18n("Perihelion")
                            value: DetailDialogLite.perihelion
                        }

                        DetailsItem {
                            label: xi18n("OrbitID")
                            value: DetailDialogLite.orbitID
                        }

                        DetailsItem {
                            label: xi18n("NEO")
                            value: DetailDialogLite.NEO
                        }

                        DetailsItem {
                            label: xi18n("Diameter")
                            value: DetailDialogLite.diameter
                        }

                        DetailsItem {
                            label: xi18n("Rotation period")
                            value: DetailDialogLite.rotation
                        }

                        DetailsItem {
                            label: xi18n("EarthMOID")
                            value: DetailDialogLite.earthMOID
                        }

                        DetailsItem {
                            label: xi18n("OrbitClass")
                            value: DetailDialogLite.orbitClass
                        }

                        DetailsItem {
                            label: xi18n("Albedo")
                            value: DetailDialogLite.albedo
                        }

                        DetailsItem {
                            label: xi18n("Dimensions")
                            value: DetailDialogLite.dimensions
                        }

                        DetailsItem {
                            label: xi18n("Period")
                            value: DetailDialogLite.period
                        }
                    }
                }
            }

            Pane {
                clip: true

                background: Rectangle {
                    anchors.fill: parent
                    color: Num.sysPalette.base
                }

                Flickable {
                    anchors.fill: parent
                    ScrollBar.vertical: ScrollBar { }
                    flickableDirection: Flickable.VerticalFlick

                    contentHeight: coordinatesCol.height

                    Column {
                        id: coordinatesCol
                        width: parent.width
                        spacing: 15

                        KSText {
                            text: xi18n("Coordinates")
                            font {
                                pointSize: 16
                            }
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        DetailsItem {
                            label: DetailDialogLite.RALabel
                            value: DetailDialogLite.RA
                        }

                        DetailsItem {
                            label: DetailDialogLite.decLabel
                            value: DetailDialogLite.dec
                        }

                        DetailsItem {
                            label: xi18n("RA (J2000.0)")
                            value: DetailDialogLite.RA0
                        }

                        DetailsItem {
                            label: xi18n("Dec (J2000.0)")
                            value: DetailDialogLite.dec0
                        }

                        DetailsItem {
                            label: xi18n("Azimuth")
                            value: DetailDialogLite.az
                        }

                        DetailsItem {
                            label: xi18n("Altitude")
                            value: DetailDialogLite.alt
                        }

                        DetailsItem {
                            label: xi18n("Hour angle")
                            value: DetailDialogLite.HA
                        }

                        DetailsItem {
                            label: xi18n("Airmass")
                            value: DetailDialogLite.airmass
                        }

                        KSText {
                            text: xi18n("Rise/Set/Transit")
                            font {
                                pointSize: 16
                            }
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        DetailsItem {
                            label: xi18n("Rise time")
                            value: DetailDialogLite.timeRise
                        }

                        DetailsItem {
                            label: xi18n("Transit time")
                            value: DetailDialogLite.timeTransit
                        }

                        DetailsItem {
                            label: xi18n("Set time")
                            value: DetailDialogLite.timeSet
                        }

                        DetailsItem {
                            label: xi18n("Azimuth at rise")
                            value: DetailDialogLite.azRise
                        }

                        DetailsItem {
                            label: xi18n("Azimuth at transit")
                            value: DetailDialogLite.altTransit
                        }

                        DetailsItem {
                            label: xi18n("Azimuth at set")
                            value: DetailDialogLite.azSet
                        }
                    }
                }
            }

            Pane {
                parent: DetailDialogLite.isLinksOn ? detailsSwipeView : null
                clip: true
                id: links

                background: Rectangle {
                    anchors.fill: parent
                    color: Num.sysPalette.base
                }

                GridLayout {
                    id: linkCol
                    rowSpacing: 15
                    anchors {
                        top: parent.top
                        left: parent.left
                        right: parent.right
                        bottom: addInfoLandscape.top
                    }

                    flow: window.isPortrait ? GridLayout.TopToBottom : GridLayout.LeftToRight

                    ColumnLayout {
                        id: infoCol

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: parent.width/2

                        spacing: 10

                        KSText {
                            id: infoLabel
                            text: xi18n("Information Links")

                            font.pointSize: 16
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Rectangle {
                            id: infoSeparator
                            Layout.fillWidth: true
                            height: 1
                            color: "grey"
                        }

                        KSListView {
                            Layout.fillHeight: true
                            Layout.fillWidth: true

                            model: DetailDialogLite.infoTitleList

                            onClicked: {
                                detailsLinkMenu.openForInfo(index)
                            }
                        }
                    }

                    ColumnLayout {
                        id: imgCol

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: parent.width/2

                        spacing: 10

                        KSText {
                            id: imgLabel
                            text: xi18n("Image Links")

                            font.pointSize: 16
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Rectangle {
                            id: imgSeparator
                            Layout.fillWidth: true
                            height: 1
                            color: "grey"
                        }

                        KSListView {
                            Layout.fillHeight: true
                            Layout.fillWidth: true

                            model: DetailDialogLite.imageTitleList

                            onClicked: {
                                detailsLinkMenu.openForImage(index)
                            }
                        }
                    }
                }

                Button {
                    id: addInfoLandscape
                    text: xi18n("Add Link")

                    anchors.bottom: parent.bottom

                    onClicked: {
                        detailsAddLink.openAdd()
                    }
                }
            }

            Pane {
                parent: DetailDialogLite.isLogOn ? detailsSwipeView : null
                clip: true

                background: Rectangle {
                    anchors.fill: parent
                    color: Num.sysPalette.base
                }

                Flickable {
                    anchors.fill: parent
                    ScrollBar.vertical: ScrollBar { }
                    flickableDirection: Flickable.VerticalFlick

                    contentHeight: logCol.height

                    Column {
                        id: logCol
                        width: parent.width
                        spacing: 15

                        KSText {
                            text: xi18n("Log")
                            font {
                                pointSize: 16
                            }
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        TextArea {
                            id: logArea
                            placeholderText: i18n("Record here observation logs and/or data on %1.", SkyMapLite.clickedObjectLite.getTranslatedName())
                            padding: 5
                            width: parent.width
                            wrapMode: TextArea.Wrap
                            text: DetailDialogLite.userLog

                            Connections {
                                target: DetailDialogLite
                                onUserLogChanged: {
                                    logArea.text = DetailDialogLite.userLog
                                }
                            }

                            onEditingFinished: {
                                DetailDialogLite.saveLogData(text)
                            }

                            background: Rectangle {
                                implicitWidth: parent.width
                                implicitHeight: 40
                                border{
                                    color: Num.sysPalette.base
                                    width: 2
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}



