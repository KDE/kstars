import QtQuick 2.6
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.4
import org.kde.kirigami 1.0 as Kirigami

ColumnLayout {
    anchors {
        left: parent.left
        right: parent.right
        top: parent.top
    }

    Text {
        id: contextText
        Layout.fillWidth: true
    }
}
