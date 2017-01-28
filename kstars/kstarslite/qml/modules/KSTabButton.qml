import QtQuick 2.6
import QtQuick.Controls 2.0
import "../constants/" 1.0
import QtQuick.Layouts 1.2

TabButton {
    id: tabButton
    width: contentItem.contentWidth//tabText.text.length * tabText.font.pixelSize

    contentItem: KSText {
        id: tabText
        text: tabButton.text
        font: tabButton.font
        opacity: enabled ? 1.0 : 0.3
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
