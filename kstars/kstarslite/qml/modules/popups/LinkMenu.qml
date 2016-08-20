import QtQuick.Controls 2.0
import QtQuick 2.7
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../helpers"

Menu {
    modal: true
    transformOrigin: Menu.Center
    padding: 5
    property int itemIndex: -1
    property bool isImage: false

    function openForImage(index) {
        isImage = true
        itemIndex = index
        open();
    }

    function openForInfo(index) {
        isImage = false
        itemIndex = index
        open();
    }

    KSMenuItem {
        text: xi18n("View resource")
        onTriggered: {
            if(isImage) {
                Qt.openUrlExternally(DetailDialogLite.getImageURL(itemIndex));
            } else {
                Qt.openUrlExternally(DetailDialogLite.getInfoURL(itemIndex));
            }
        }
    }

    KSMenuItem {
        text: xi18n("Edit")
        onTriggered: {
            addLinkPopup.openEdit(itemIndex, isImage)
        }
    }

    KSMenuItem {
        text: xi18n("Delete")
        onTriggered: {
            DetailDialogLite.removeLink(itemIndex, isImage)
        }
    }
}

