// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick.Controls 2.1
import QtQuick.Controls 1.4
import QtQuick 2.7
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../../modules"

KSPage {
    id: addLink
    title: editMode ? xi18n("%1 - Edit Link", SkyMapLite.clickedObjectLite.getTranslatedName()) : xi18n("%1 - Add a Link", SkyMapLite.clickedObjectLite.getTranslatedName())
    property bool editMode: false // true if popup is in edit mode. False if in add mode
    property bool isImage: false //is the object for which this popup was opened an image or information
    property int itemIndex: -1
    property TextField description: descField
    property TextField url: urlField

    function openAdd() {
        editMode = false
        stackView.push(this)
    }

    function openEdit(index, isImage) {
        editMode = true
        if(!isImage) {
            descField.text = DetailDialogLite.infoTitleList[index]
            urlField.text = DetailDialogLite.getInfoURL(index)
        } else {
            descField.text = DetailDialogLite.imageTitleList[index]
            urlField.text = DetailDialogLite.getImageURL(index)
        }

        itemIndex = index
        stackView.push(this)
    }

    /**
      closes the page and clears all text fields
    */
    function closeAddLink() {
        descField.clear()
        urlField.clear()
        stackView.pop()
    }

    ColumnLayout {
        anchors {
            left: parent.left
            right: parent.right
        }

        RowLayout {
            visible: !editMode
            RadioButton {
                checked: true //Set this button to be checked initially
                id: radioInfo
                text: xi18n("Information")
            }
            RadioButton {
                id: radioImg
                text: xi18n("Image")
            }
        }

        KSLabel {
            text: xi18n("Description")
        }

        KSTextField {
            id: descField
            Layout.fillWidth: true
        }

        KSLabel {
            text: xi18n("URL")
        }

        KSTextField {
            id: urlField
            Layout.fillWidth: true
        }

        RowLayout {
            Button {
                //enabled:
                text: editMode ? xi18n("Save") : xi18n("Add")
                onClicked: {
                    if(descField.text == "" || urlField.text == "") {
                        skyMapLite.notification.showNotification(xi18n("Please, fill in URL and Description"))
                        return
                    }

                    if(editMode) {
                        DetailDialogLite.editLink(itemIndex, isImage, descField.text, urlField.text)
                    } else {
                        DetailDialogLite.addLink(urlField.text, descField.text, radioImg.checked)
                    }
                    closeAddLink()
                }
            }

            Button {
                text: xi18n("Cancel")
                onClicked: {
                    closeAddLink()
                }
            }
        }
    }
}
