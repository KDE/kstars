// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
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
    title: xi18n("Find an Object")
    onVisibleChanged: {
        if(visible) {
            typeChoose.currentIndex = 0
            searchQuery.text = ""
        }
    }

    ColumnLayout {
        id: findColumn
        anchors.fill: parent
        spacing: 5 * Num.dp
        anchors{
            bottom: parent.bottom
            bottomMargin: 15 * Num.dp
        }

        RowLayout {
            anchors {
                left: parent.left
                right: parent.right
            }
            KSLabel {
                text: xi18n("Filter by name: ")
            }
            KSTextField {
                id: searchQuery
                Layout.fillWidth: true
                onTextChanged: {
                    FindDialogLite.filterList(text)
                }
            }
        }

        RowLayout {
            anchors {
                left: parent.left
                right: parent.right
            }
            KSLabel {
                text: xi18n("Filter by type: ")
            }

            Item {
                //Spacer
                Layout.minimumWidth: 30 * Num.dp
                Layout.fillWidth: true
            }

            ComboBox {
                id: typeChoose
                model: FindDialogLite.filterModel
                Layout.fillWidth: true

                //Init list with objects when everything is loaded
                Connections {
                    target: window
                    onLoaded: {
                        if(isLoaded) FindDialogLite.filterByType(typeChoose.currentIndex)
                    }
                }

                onCurrentIndexChanged: {
                    if(isLoaded) FindDialogLite.filterByType(currentIndex)
                }
            }
        }

        KSListView {
            model: SortModel
            textRole: "name"

            Layout.fillWidth: true
            Layout.fillHeight: true

            onClicked: {
                stackView.replace(null, initPage)
                FindDialogLite.selectObject(index)
            }
        }

        RowLayout {
            KSButton {
                id: searchInInternet
                enabled: searchQuery.text.length > 0 && FindDialogLite.isResolveEnabled

                text: xi18n("Search in internet")
                onClicked: {
                    FindDialogLite.resolveInInternet(searchQuery.text)
                }
            }

            KSButton {
                text: xi18n("Cancel")
                onClicked: {
                    stackView.pop()
                }
            }
        }
    }
}

