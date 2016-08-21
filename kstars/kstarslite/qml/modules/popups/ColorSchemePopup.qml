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

    function formatColorScheme(schemeName) {
        return schemeName.substring(3) + ".colors"
    }

    KSListView {
        id: colorsList
        anchors.centerIn: parent
        checkCurrent: true

        model: ListModel {
            id: colorsModel

            Component.onCompleted: {
                append({ name: xi18n("Classic"), scheme: "cs_classic" });
                append({ name: xi18n("Star Chart"), scheme: "cs_chart" });
                append({ name: xi18n("Night Vision"), scheme: "cs_night" });
                append({ name: xi18n("Moonless Night"), scheme: "cs_moonless-night" });

                //Set current index to current scheme color
                var currentScheme = KStarsData.colorSchemeName()
                for(var i = 0; i < colorsList.model.count; ++i) {
                    if(formatColorScheme(colorsList.model.get(i).scheme) == currentScheme) {
                       colorsList.currentIndex = i
                    }
                }
            }
        }

        onClicked: {
            var item = colorsModel.get(colorsList.currentIndex)

            KStarsLite.loadColorScheme(formatColorScheme(item.scheme));
            notification.showNotification("Set color scheme to " + item.name)

            colorSchemeChanged()
            close()
        }

        textRole: "name"
    }
}
