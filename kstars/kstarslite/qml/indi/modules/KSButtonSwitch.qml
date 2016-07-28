import QtQuick 2.4
import QtQuick.Controls 1.4

Button {
    property string switchName: ""
    property Flow parentRow
    checkable: true
    checked: true

    onClicked: {
        parentRow.sendNewSwitch(switchName, null)
        Qt.inputMethod.hide()
    }
}
