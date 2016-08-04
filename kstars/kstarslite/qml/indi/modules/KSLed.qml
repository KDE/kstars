import QtQuick 2.4
import QtQuick.Layouts 1.2
import QtQuick.Controls 1.4
import org.kde.kirigami 1.0 as Kirigami
import "../../constants" 1.0
import "../../modules"

Row {
    spacing: 5
    id: ledRow
    property string deviceName
    property string propName
    property string label
    property string name //Used in Light

    onDeviceNameChanged: {
        syncLEDProperty()
    }

    onPropNameChanged: {
        syncLEDProperty()
    }

    function syncLEDProperty() {
        led.color = ClientManagerLite.syncLED(ledRow.deviceName, ledRow.propName)
    }

    function syncLEDLight() {
        led.color = ClientManagerLite.syncLED(ledRow.deviceName, ledRow.propName, ledRow.name)
    }

    Connections {
        target: ClientManagerLite
        onNewINDILight: {
            if(ledRow.deviceName == deviceName) {
                if(ledRow.propName == propName) {
                    ledRow.syncLEDLight() // We update only Lights here
                }
            }
        }
    }

    Led {
        id: led
        color: "red"
        anchors.verticalCenter: parent.verticalCenter
    }

    Kirigami.Label {
        text: ledRow.label
        anchors.verticalCenter: parent.verticalCenter
        color: num.sysPalette.text
    }
}
