// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.6
import QtQuick.Window 2.2
import "../modules"
import "../constants" 1.0
import QtQuick.Layouts 1.2
import QtQuick.Controls 2.0
import QtQuick.Dialogs 1.2 as Dialogs

KSPage {
    id: imagePreview
    anchors.fill: parent
    title: xi18n("Image Preview - %1", deviceName)

    property string deviceName
    property Item buttonRow: null

    Item {
        id: imgPreviewColumn
        anchors.fill: parent

        RowLayout {
            id: saveButtons
            anchors {
                top: parent.top
                left: parent.left
             //   margins: 10 * Num.dp
            }
            Layout.fillWidth: true

            spacing: 5 * Num.dp

            Button {
                text: xi18n("Save As")

                onClicked: {
                    ClientManagerLite.saveDisplayImage()
                }
            }
        }

        Image {
            id: image
            Layout.fillHeight: true
            Layout.fillWidth: true
            fillMode: Image.PreserveAspectFit
            anchors {
                top: saveButtons.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom

                margins: 15 * Num.dp
            }
        }

        Connections {
            target: ClientManagerLite

            onNewINDIBLOBImage: {
                if(imagePreview.deviceName == deviceName) {
                    image.source = "image://images/ccdPreview"
                    stackView.push(imagePreview)
                }
            }

            onCreateINDIButton: {
                if(imagePreview.deviceName == deviceName) {
                    if(propName == "UPLOAD_MODE") {
                        if(imagePreview.buttonRow == null) {
                            var buttonRowComp = Qt.createComponent("modules/KSButtonsSwitchRow.qml");
                            imagePreview.buttonRow = buttonRowComp.createObject(saveButtons)
                            imagePreview.buttonRow.deviceName = deviceName
                            imagePreview.buttonRow.propName = propName
                            imagePreview.buttonRow.exclusive = exclusive
                        }
                        imagePreview.buttonRow.addButton(propText, switchName, checked, enabled)
                    }
                }
            }

            onRemoveINDIProperty: {
                if(imagePreview.deviceName == deviceName) {
                    if(propName == "UPLOAD_MODE") {
                        if(imagePreview.buttonRow != null) {
                            imagePreview.buttonRow.destroy()
                            imagePreview.buttonRow = null
                        }
                    }
                }
            }
        }

    }
}
