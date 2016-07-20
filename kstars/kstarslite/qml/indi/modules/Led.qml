import QtQuick 2.4
import "../../constants" 1.0

Image {
    property string color: "grey"
    width: 16 * num.dp
    height: 16 * num.dp
    sourceSize.height: 16 * num.dp
    sourceSize.width: 16 * num.dp

    source: "images/" + num.density + "/grey.png"
    onColorChanged: {
        if(color == "red") {
            source = "images/" + num.density + "/red.png"
        } else if(color == "green") {
            source = "images/" + num.density + "/green.png"
        } else if(color == "yellow") {
            source = "images/" + num.density + "/yellow.png"
        } else if(color == "grey") {
            source = "images/" + num.density + "/grey.png"
        }
    }
}
