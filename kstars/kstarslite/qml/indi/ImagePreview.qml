import QtQuick 2.4
import QtQuick.Window 2.2
import "../modules"
import "../constants" 1.0
import QtQuick.Layouts 1.2
import QtQuick.Controls 1.4 as Controls
import org.kde.kirigami 1.0 as Kirigami
import QtQuick.Dialogs 1.2 as Dialogs

KSPage {
    id: imagePreview
    contentItem: imgPreviewColumn
    title: "Image Preview - " + deviceName

    property string deviceName: title
    property Item buttonRow

    Item {
        id: imgPreviewColumn
        anchors.bottom: parent.bottom

        RowLayout {
            id: saveButtons
            anchors {
                top: parent.top
                left: parent.left
                margins: 10 * num.dp
            }
            Layout.fillWidth: true

            spacing: 5 * num.dp
            Controls.Button {
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

    }

    Connections {
        target: ClientManagerLite
        onNewINDIBLOBImage: {
            image.source = "image://images/ccdPreview"
            imagePreview.showPage()
        }
        onCreateINDIButton: {
            if(deviceName == imagePreview.deviceName) {
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
    }
}
