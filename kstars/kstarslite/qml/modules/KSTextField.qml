import QtQuick 2.6
import QtQuick.Controls 2.0
import "../constants/" 1.0

TextField {
    id: control

    color: num.sysPalette.text
    selectedTextColor: num.sysPalette.highlightedText
    selectionColor: num.sysPalette.highlight

    background: Rectangle {
        y: control.height - height - control.bottomPadding / 2
        implicitWidth: 120
        height: control.activeFocus ? 2 : 1
        color: control.activeFocus ? num.sysPalette.text : num.sysPalette.dark
    }
}
