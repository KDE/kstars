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
import "../modules"
import "../constants" 1.0
import QtQuick.Layouts 1.2
import QtQuick.Controls 2.0
import QtQuick.Dialogs 1.2 as Dialogs

KSPage {
    id: imagePreview
    contentItem: imgPreviewColumn
    title: "Image Preview - " + deviceName

    property string deviceName
    property Item buttonRow: null

    Item {
        id: imgPreviewColumn
        anchors.bottom: parent.bottom

        RowLayout {
            id: saveButtons
            anchors {
                top: parent.top
                left: parent.left
             //   margins: 10 * num.dp
            }
            Layout.fillWidth: true

            spacing: 5 * num.dp

            Button {
                text: "Save As"

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

                margins: 15 * num.dp
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
