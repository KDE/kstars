import QtQuick 2.6
import QtQuick.Controls 2.0

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
