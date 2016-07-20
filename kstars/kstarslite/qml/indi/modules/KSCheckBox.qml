import QtQuick 2.4
import QtQuick.Controls 1.4

CheckBox {
    property string switchName: ""
    property Row parentRow

    onClicked: {
        parentRow.sendNewSwitch(switchName, null)
    }
}
