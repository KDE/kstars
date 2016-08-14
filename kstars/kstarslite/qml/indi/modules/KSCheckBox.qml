import QtQuick 2.6
import QtQuick.Controls 2.0

CheckBox {
    property string switchName: ""
    property Flow parentRow

    onClicked: {
        parentRow.sendNewSwitch(switchName, null)
    }
}
