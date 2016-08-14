import QtQuick 2.6
import QtQuick.Controls 2.0
import QtGraphicalEffects 1.0
import "../../constants/" 1.0

AbstractButton {
    id: button
    property string iconSrc: ""
    width: icon.width + 5
    height: icon.height + 5
    property bool toggled: false

    onClicked: {
        toggled = !toggled
    }


    Image {
        id: icon
        source: iconSrc
        width: sourceSize.width/num.pixelRatio
        height: sourceSize.height/num.pixelRatio
        anchors.centerIn: iconRect
    }

    background: Rectangle {
        id: iconRect
        radius: 5
        anchors {
            fill: parent
        }
        color: "black"
        border {
            color: toggled ? num.sysPalette.highlight : "grey"
            width: 1
        }
    }

    onDownChanged: {
        if(down) opacity = 0.6
        else opacity = 1
    }

    onPressed: {
        opacity = 0.6
    }

    onReleased: {
        opacity = 1
    }

    Behavior on opacity {
        OpacityAnimator { duration: 100 }
    }
}
