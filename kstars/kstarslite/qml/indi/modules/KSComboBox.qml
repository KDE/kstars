import QtQuick 2.4
import QtQuick.Controls 1.4

ComboBox {
    property string deviceName: ""
    property string propName: ""

    Connections {
        target: ClientManagerLite
        onNewINDISwitch: {
            if(deviceName == device) {
                if(name == propName) {
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
            ClientManagerLite.sendNewINDISwitch(deviceName,propName,index)
        }
    }

    model: ListModel {

    }
}
