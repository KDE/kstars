// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

pragma Singleton
import QtQuick 2.7
import QtQuick.Window 2.2

QtObject {
    property double dpi: Screen.pixelDensity * 25.4
    property double dpmm: Screen.pixelDensity
    property double dp: dpi < 160 ? 1 : dpi/160
    property double pixelRatio: Screen.devicePixelRatio
    property string density: {
        if(dpi * pixelRatio <= 120) {
            return "ldpi"
        }
        else if(dpi * pixelRatio <=160) {
            return "mdpi"
        }
        else if(dpi * pixelRatio <= 240) {
            return "hdpi"
        }
        else if(dpi * pixelRatio <= 320) {
            return "xhdpi"
        }
        else if(dpi * pixelRatio <= 480) {
            return "xxhdpi"
        }
        else if(dpi * pixelRatio <=640) {
            return "xxxhdpi"
        }
    }
    property string iconpath: "../" + "images/"  + density + "/icons/"
    property string imagesPath: "images/"  + density + "/"
    property SystemPalette sysPalette: SystemPalette { }

    property int marginsKStab: 5 * dp  //Margins of KSTab content
}
