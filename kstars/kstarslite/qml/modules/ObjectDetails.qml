import QtQuick 2.7
import QtQuick.Window 2.2
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import "helpers"

KSPage {
    title: SkyMapLite.clickedObjectLite.translatedName + " - " + tabBar.currentItem.text
    ColumnLayout {
        anchors.fill: parent

        TabBar {
            id: tabBar
            currentIndex: 0
            Layout.fillWidth: true

            TabButton {
                text: xi18n("General")
            }

            TabButton {
                text: xi18n("Position")
            }

            TabButton {
                text: xi18n("Links")
            }

            TabButton {
                text: xi18n("Advanced")
            }

            TabButton {
                text: xi18n("Log")
            }
        }

        SwipeView {
            id: deviceSwipeView
            Layout.fillHeight: true
            Layout.fillWidth: true
            currentIndex: tabBar.currentIndex
            clip: true


            onCurrentIndexChanged: {
                tabBar.currentIndex = currentIndex
            }

            KSTab {
                id: general
                rootItem: Column {
                    width: general.width
                    spacing: 15

                    Text {
                        text: DetailDialogLite.name
                        font {
                            pointSize: 16
                        }
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Image {
                        source: DetailDialogLite.thumbnail
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: DetailDialogLite.typeInConstellation
                        anchors.horizontalCenter: parent.horizontalCenter
                        font.pointSize: 12
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

                }
            }

            KSTab {

            }

            KSTab {

            }

            KSTab {

            }

            KSTab {

            }
        }
    }
}


