import QtQuick 2.4
import QtQuick.Window 2.2

Window {
    id: splash
    color: "transparent"
    title: "Splash Window"
    modality: Qt.ApplicationModal
    flags: Qt.SplashScreen
    signal timeout

    x: (Screen.width - splashImage.width) / 2
    y: (Screen.height - splashImage.height) / 2

    width: splashImage.width
    height: splashImage.height

    Image {
        id: splashImage
        source: "images/kstars.png"

        Text {
            id: progress
            color: "#FFF"

            anchors {
                top: parent.top
                left: parent.left
                margins: 10
            }
        }
    }

    Connections {
        target: KStarsData
        onProgressText: {
            progress.text = text
        }
    }

    Connections {
        target: KStarsLite
        onShowSplash: {
            visible = true
        }
        onDataLoadFinished: {
            visible = false
            splash.timeout()
        }
    }
}

