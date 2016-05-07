import QtQuick 2.0
import QtGraphicalEffects 1.0
import "../../constants/" 1.0

Item {
    property string iconSrc: ""
    width: iconRect.width
    height: iconRect.height

    Image {
        id: icon
        source: iconSrc
        visible: false
    }

    Rectangle {
        width: iconRect.width + num.dp * 3
        height: iconRect.height + num.dp *3
        color: "gray"

        radius: 5
        Rectangle {
            id: iconRect
            width: icon.width
            height: icon.height
            radius: 5
            anchors.centerIn: parent
            color: "black"
        }
        OpacityMask {
            anchors.fill: iconRect
            source: icon
            maskSource: iconRect
        }
    }


}
