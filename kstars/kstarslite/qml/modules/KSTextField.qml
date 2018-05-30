import QtQuick 2.6
import QtQuick.Controls 2.0
import "../constants/" 1.0

TextField {
    id: control

    color: Num.sysPalette.text
    selectedTextColor: Num.sysPalette.highlightedText
    selectionColor: Num.sysPalette.highlight

    background: Rectangle {
        y: control.height - height - control.bottomPadding / 2
        implicitWidth: 120
        height: control.activeFocus ? 2 : 1
        color: control.activeFocus ? Num.sysPalette.text : Num.sysPalette.dark
    }
}
