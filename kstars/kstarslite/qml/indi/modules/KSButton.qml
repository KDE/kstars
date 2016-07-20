import QtQuick 2.4
import QtQuick.Controls 1.4

Button {
    property string switchName: ""
    property Row parentRow
    checkable: true

    onClicked: {
        parentRow.sendNewSwitch(switchName, null)
        checked = true
    }
}
