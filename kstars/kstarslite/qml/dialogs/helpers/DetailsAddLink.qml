import QtQuick.Controls 2.0
import QtQuick 2.7
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../../modules" //Import modules

KSPage {
    id: addLink
    title: SkyMapLite.clickedObjectLite.getTranslatedName() + " - " + (editMode ? xi18n("Edit Link") : xi18n("Add a Link") )
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
        linkPopup.isImage = isImage
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

        Label {
            text: xi18n("Description")
        }

        TextField {
            id: descField
            Layout.fillWidth: true
        }

        Label {
            text: xi18n("URL")
        }

        TextField {
            id: urlField
            Layout.fillWidth: true
        }

        RowLayout {
            Button {
                //enabled:
                text: editMode ? xi18n("Save") : xi18n("Add")
                onClicked: {
                    if(descField.text == "" || urlField.text == "") {
                        notification.showNotification(xi18n("Please, fill in URL and Description"))
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
