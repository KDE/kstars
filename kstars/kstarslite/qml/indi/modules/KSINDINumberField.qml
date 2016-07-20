import QtQuick 2.4
import QtQuick.Layouts 1.2
import "../../constants" 1.0
import org.kde.kirigami 1.0 as Kirigami

import QtQuick 2.4
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.4

RowLayout {
    Layout.fillWidth: true
    Layout.fillHeight: true
    property Item textField: field
    TextField {
        id: field
    }
    Button {
        text:"Set"
    }
}
