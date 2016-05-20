import QtQuick 2.4
import QtQuick.Window 2.2
import "../constants"

Item {
    id: splash
    //color: "transparent"
    //title: "Splash Window"
    //modality: Qt.ApplicationModal
    //flags: Qt.SplashScreen
    signal timeout

    x: (Screen.width - splashImage.width) / 2
    y: (Screen.height - splashImage.height) / 2

    width: splashImage.width
    height: splashImage.height

    Timer {
        interval: 3500;
        running: true;
        onTriggered: {
            splash.timeout();
            visible = false
        }
    }

    Timer {
        id: progressText
        interval: 500
        running: true
        repeat: true
    }

    Image {
        id: splashImage
        source: "images/" + num.density + "/icons/icon.png"

        Text {
            id: progress
            color: "#FFF"

            Connections {
                target: progressText
                onTriggered: {
                    progress.text = progress.text + "a"
                }
            }

            anchors {
                top: parent.top
                left: parent.left
                margins: 10
            }
        }
    }
/*
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
    }*/
}

