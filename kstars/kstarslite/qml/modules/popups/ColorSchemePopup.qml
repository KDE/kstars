import QtQuick.Controls 2.0
import QtQuick 2.6
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../../modules"

Popup {
    //Change it to Popup when it will become more stable
    id: colorSPopup
    focus: true
    modal: true
    height: parent.height > colorsList.implicitHeight ? colorsList.implicitHeight : parent.height
    signal colorSchemeChanged()

    KSListView {
        id: colorsList
        anchors.centerIn: parent

        model: ListModel {
            id: colorsModel

            Component.onCompleted: {
                append({ name: xi18n("Classic"), scheme: "cs_classic" });
                append({ name: xi18n("Star Chart"), scheme: "cs_chart" });
                append({ name: xi18n("Night Vision"), scheme: "cs_night" });
                append({ name: xi18n("Moonless Night"), scheme: "cs_moonless-night" });
            }
        }

        onClicked: {
            var item = colorsModel.get(colorsList.currentIndex)
            KStarsLite.loadColorScheme(item.scheme.substring(3) + ".colors");
            notification.showNotification("Set color scheme to " + item.name)
            colorSchemeChanged()
            close()
        }

        textRole: "name"
    }
}
