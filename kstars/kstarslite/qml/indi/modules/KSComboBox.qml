import QtQuick 2.4
import QtQuick.Controls 1.4

ComboBox {
    id: comboBox
    property string deviceName: ""
    property string propName: ""

    Connections {
        target: ClientManagerLite
        onNewINDISwitch: {
            if(comboBox.deviceName == deviceName) {
                if(comboBox.propName == propName) {
                    for(var i = 0; i < model.count; ++i) {
                        if(model.get(i).name == switchName && isOn) {
                            currentIndex = i;
                            break;
                        }
                    }
                }
            }
        }
    }

    onActivated: {
        if(index >= 0) {
            ClientManagerLite.sendNewINDISwitch(comboBox.deviceName, comboBox.propName,index)
        }
    }

    model: ListModel {

    }
}
