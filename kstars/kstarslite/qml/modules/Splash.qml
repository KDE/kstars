import QtQuick 2.6
import QtQuick.Window 2.2
import "../constants"

//Rectangle - to allow z-index ordering (for some reason it doesn't work with plain Item)
Rectangle {
    id: splash
    signal timeout
    state: "Inivisble"

    Image {
        id: splashImage
        source: "images/" + num.density + "/splash.png"
        anchors.centerIn: parent

        width: sourceSize.width/num.pixelRatio
        height: sourceSize.height/num.pixelRatio

        Text {
            id: progress
            color: "#000"

            anchors {
                top: parent.bottom
                horizontalCenter: parent.horizontalCenter
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
            splash.state = "Visible"
        }
        onDataLoadFinished: {
            splash.timeout()
            splash.state = "Invisible"
        }
    }

    states: [
        State{
            name: "Visible"
            PropertyChanges{target: splash; opacity: 1.0}
            PropertyChanges{target: splash; visible: true}
        },
        State{
            name:"Invisible"
            PropertyChanges{target: splash; opacity: 0.0}
            PropertyChanges{target: splash; visible: false}
        }
    ]

    transitions: [
            Transition {
                from: "Visible"
                to: "Invisible"

                SequentialAnimation{
                   NumberAnimation {
                       target: splash
                       property: "opacity"
                       duration: 800
                       easing.type: Easing.InOutQuad
                   }
                   NumberAnimation {
                       target: splash
                       property: "visible"
                       duration: 0
                   }
                }
            }
        ]
}

