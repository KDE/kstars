pragma Singleton
import QtQuick 2.0
import QtQuick.Window 2.0

QtObject {
    property double dpi: Screen.pixelDensity * 25.4
    property double dpmm: Screen.pixelDensity
    property double dp: dpi < 160 ? 1 : dpi/160
    property string density: {
        if(dpi <= 120) {
            return "ldpi"
        }
        else if(dpi <=160) {
            return "mdpi"
        }
        else if(dpi <= 240) {
            return "hdpi"
        }
        else if(dpi <= 320) {
            return "xhdpi"
        }
        else if(dpi <= 480) {
            return "xxhdpi"
        }
        else if(dpi <=640) {
            return "xxxhdpi"
        }
    }
    property string iconpath: "../" + "images/"  + density + "/icons/"
    property string imagesPath: "images/"  + density + "/"
    property SystemPalette sysPalette: SystemPalette { }
}
