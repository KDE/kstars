import QtQuick.Controls 2.0
import QtQuick 2.7
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../../modules"
import QtPositioning 5.2

Popup {
    x: (window.width - width) / 2
    y: window.height / 6
    focus: true
    modal: true

    ColumnLayout {
        id: aboutDialog
        focus: true
        width: Math.min(window.width, window.height) / 3 * 2

        Column {
            id: loadingColumn
            width: parent.width

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Label {
                id: fetchText
                width: parent.width
                wrapMode: Label.Wrap
                horizontalAlignment: Label.AlignHCenter
                text: xi18n("Please, wait while we are fetching coordinates")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: xi18n("Cancel")
                onClicked: {
                    close()
                }
            }
        }
    }
}
