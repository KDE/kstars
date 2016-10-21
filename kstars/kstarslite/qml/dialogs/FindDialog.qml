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
    contentItem: findColumn
    title: xi18n("Find an Object")
    onVisibleChanged: {
        if(visible) {
            typeChoose.currentIndex = 0
            searchQuery.text = ""
        }
    }

    ColumnLayout {
        id: findColumn
        spacing: 5 * num.dp
        anchors{
            bottom: parent.bottom
            bottomMargin: 15 * num.dp
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
                text: "Filter by type: "
            }

            Item {
                //Spacer
                Layout.minimumWidth: 30 * num.dp
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
            Button {
                id: searchInInternet
                enabled: searchQuery.text.length > 0 && FindDialogLite.isResolveEnabled

                text: "Search in internet"
                onClicked: {
                    FindDialogLite.resolveInInternet(searchQuery.text)
                }
            }

            Button {
                text: "Cancel"
                onClicked: {
                    stackView.pop()
                }
            }
        }
    }
}

