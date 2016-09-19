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
import QtQuick.Controls 2.0

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
