import QtQuick 2.6
import QtQuick.Controls 2.0
import "../constants/" 1.0

TabButton {
    id: tabButton
    contentItem: KSText {
        text: tabButton.text
        font: tabButton.font
        opacity: enabled ? 1.0 : 0.3
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
