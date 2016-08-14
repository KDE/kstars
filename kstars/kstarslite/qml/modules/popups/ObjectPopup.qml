import QtQuick.Controls 2.0
import QtQuick 2.7
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../helpers"

Menu {
    modal: true
    x: parent.width - width
    transformOrigin: Menu.Center
    padding: 5

    Column {
        width: parent.width
        spacing: 10

        Text {
            id: objectName
            text: SkyMapLite.clickedObjectLite.translatedName
            font.pointSize: 12
            anchors {
                left: parent.left
                leftMargin: 10
            }
        }

        Rectangle {
            color: "grey"
            width: parent.width - 10
            height: 1
            anchors {
                horizontalCenter: parent.horizontalCenter
            }
        }
    }

    KSMenuItem {
        text: xi18n("Center and Track")
        onTriggered: settingsPopup.open()
    }
    KSMenuItem {
        text: xi18n("Details")
        onTriggered: stackView.push(objectDetails)
    }
}
