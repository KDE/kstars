import QtQuick 2.5
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.1

Rectangle {
    id: rectangle
    width: 200
    height: 480
    color: "#000000"

    property color buttonColor: "gainsboro"

    ColumnLayout {
        id: mainVerticalLayout
        anchors.fill: parent


        GridLayout {
            id: mountMotionLayout
            width: 200
            height: 150
            Layout.minimumHeight: 150
            Layout.minimumWidth: 200
            Layout.maximumHeight: 150
            Layout.maximumWidth: 200
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillWidth: true
            rows: 3
            columns: 3


            Button
            {
                id: northWest
                width: 52
                height: 54
                Layout.fillHeight: true
                Layout.fillWidth: true

                background: Rectangle
                {
                    color: northWest.pressed ? "red" : rectangle.buttonColor
                }

                onPressAndHold:
                {
                    mount.motionCommand(0, 0, 0);
                }

                onReleased:
                {
                    mount.motionCommand(1, 0, 0);
                }

                Image {
                    width: 50
                    anchors.fill: parent
                    source: "go-northwest.png"
                }
            }

            Button {
                id: north
                width: 52
                height: 54
                Layout.fillHeight: true
                Layout.fillWidth: true

                background: Rectangle
                {
                    color: north.pressed ? "red" : rectangle.buttonColor
                }

                onPressAndHold:
                {
                    mount.motionCommand(0, 0, -1);
                }

                onReleased:
                {
                    mount.motionCommand(1, 0, -1);
                }

                Image {
                    width: 50
                    anchors.fill: parent
                    source: "go-north.png"
                }
            }

            Button {
                id: northEast
                width: 52
                height: 54
                Layout.fillHeight: true
                Layout.fillWidth: true


                background: Rectangle
                {
                    color: northEast.pressed ? "red" : rectangle.buttonColor
                }

                onPressAndHold:
                {
                    mount.motionCommand(0, 0, 1);
                }

                onReleased:
                {
                    mount.motionCommand(1, 0, 1);
                }


                Image {
                    width: 50
                    anchors.fill: parent
                    source: "go-northeast.png"
                }
            }





            Button {
                id: west
                width: 52
                height: 54
                Layout.fillHeight: true
                Layout.fillWidth: true

                background: Rectangle
                {
                    color: west.pressed ? "red" : rectangle.buttonColor
                }

                onPressAndHold:
                {
                    mount.motionCommand(0, -1, 0);
                }

                onReleased:
                {
                    mount.motionCommand(1, -1, 0);
                }


                Image {
                    width: 50
                    anchors.fill: parent
                    source: "go-west.png"
                }
            }

            Button {
                id: stop
                width: 52
                height: 54
                Layout.fillHeight: true
                Layout.fillWidth: true

                background: Rectangle
                {
                    color: stop.pressed ? "red" : rectangle.buttonColor
                }

                Image {
                    width: 50
                    anchors.fill: parent
                    source: "stop.png"
                }

                onClicked:
                {
                    mount.abort();
                }


            }

            Button {
                id: east
                width: 52
                height: 54
                Layout.fillHeight: true
                Layout.fillWidth: true

                background: Rectangle
                {
                    color: east.pressed ? "red" : rectangle.buttonColor
                }

                onPressAndHold:
                {
                    mount.motionCommand(0, -1, 1);
                }

                onReleased:
                {
                    mount.motionCommand(1, -1, 1);
                }


                Image {
                    width: 50
                    anchors.fill: parent
                    source: "go-east.png"
                }
            }

            Button {
                id: southWest
                width: 52
                height: 54
                Layout.fillHeight: true
                Layout.fillWidth: true

                background: Rectangle
                {
                    color: southWest.pressed ? "red" : rectangle.buttonColor
                }

                onPressAndHold:
                {
                    mount.motionCommand(0, 1, 0);
                }

                onReleased:
                {
                    mount.motionCommand(1, 1, 0);
                }


                Image {
                    width: 50
                    anchors.fill: parent
                    source: "go-southwest.png"
                }
            }

            Button {
                id: south
                width: 52
                height: 54
                Layout.fillHeight: true
                Layout.fillWidth: true

                background: Rectangle
                {
                    color: south.pressed ? "red" : rectangle.buttonColor
                }

                onPressAndHold:
                {
                    mount.motionCommand(0, 1, -1);
                }

                onReleased:
                {
                    mount.motionCommand(1, 1, -1);
                }


                Image {
                    width: 50
                    anchors.fill: parent
                    source: "go-south.png"
                }
            }

            Button {
                id: southEast
                width: 52
                height: 54
                Layout.fillHeight: true
                Layout.fillWidth: true

                background: Rectangle
                {
                    color: southEast.pressed ? "red" : rectangle.buttonColor
                }

                onPressAndHold:
                {
                    mount.motionCommand(0, 1, 1);
                }

                onReleased:
                {
                    mount.motionCommand(1, 1, 1);
                }

                Image {
                    width: 50
                    anchors.fill: parent
                    source: "go-southeast.png"
                }
            }


        }


        RowLayout {
            id: mountSpeedLayout
            width: 200
            height: 100
            Layout.maximumWidth: 200
            Layout.fillHeight: false
            Layout.fillWidth: true

            Slider {
                id: speedSlider
                width: 150
                objectName: "speedSliderObject"
                Layout.fillWidth: true
                stepSize: 1
                from: 0
                to: 4
                value: 0

                onValueChanged:
                {
                    mount.setSlewRate(speedSlider.value)
                }
            }

            Label {
                id: speedLabel
                width: 75
                objectName: "speedLabelObject"
                text: qsTr("1x")
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                Layout.maximumWidth: 75
                Layout.minimumWidth: 75
                font.weight: Font.Bold
                font.bold: true
                font.pointSize: 18
                font.family: "Verdana"
                fontSizeMode: Text.Fit
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
                color: "white"
            }
        }


        GridLayout {
            id: mountCoordsLayout
            width: 100
            height: 100
            Layout.fillHeight: false
            Layout.maximumWidth: 200
            Layout.fillWidth: true
            rows: 2
            columns: 4

            Label {
                id: raLabel
                text: qsTr("RA:")
                font.bold: true
                font.pointSize: 12
                color: "white"
            }

            Label {
                id: raValue
                objectName: "raValueObject"
                width: 50
                color: "#ffffff"
                text: "00:00:00"
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: azLabel
                color: "#ffffff"
                text: qsTr("AZ:")
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: azValue
                objectName: "azValueObject"
                width: 50
                color: "#ffffff"
                text: "00:00:00"
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: deLabel
                color: "#ffffff"
                text: qsTr("DE:")
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: deValue
                objectName: "deValueObject"
                width: 50
                color: "#ffffff"
                text: "00:00:00"
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: altLabel
                color: "#ffffff"
                text: qsTr("AL:")
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: altValue
                objectName: "altValueObject"
                width: 50
                color: "#ffffff"
                text: "00:00:00"
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: haLabel
                color: "#ffffff"
                text: qsTr("HA:")
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: haValue
                objectName: "haValueObject"
                width: 50
                color: "#ffffff"
                text: "00:00:00"
                font.pointSize: 12
                font.bold: true

            }

            Label {
                id: zaLabel
                color: "#ffffff"
                text: qsTr("ZA:")
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: zaValue
                objectName: "zaValueObject"
                width: 50
                color: "#ffffff"
                text: "00:00:00"
                font.pointSize: 12
                font.bold: true

            }
        }


        RowLayout {
            id: targetNameLayout
            width: 200
            height: 100
            Layout.fillHeight: false
            Layout.maximumWidth: 200
            Layout.minimumWidth: 200
            Layout.fillWidth: true

            Label {
                id: targetLabel
                color: "#ffffff"
                text: qsTr("Target:")
                verticalAlignment: Text.AlignVCenter
                Layout.fillHeight: true
                Layout.fillWidth: true
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: targetText
                color: "#ffffff"
                text: ""
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 75
                font.pointSize: 12
                font.bold: true
            }

            Button {
                id: findButton
                width: 32
                height: 32
                text: qsTr("Find")
                Layout.maximumHeight: 32
                Layout.minimumHeight: 32
                Layout.minimumWidth: 32
                Layout.maximumWidth: 32
            }
        }


        GridLayout {
            id: targetCoordsLayout
            width: 100
            height: 100
            Layout.fillWidth: true
            rows: 2
            columns: 2

            Label {
                id: targetRALabel
                color: "#ffffff"
                text: qsTr("RA (JNOW):")
                font.pointSize: 12
                font.bold: true
            }

            TextField {
                id: targetRAText
                width: 150
                Layout.minimumHeight: 30
                Layout.maximumHeight: 30
                Layout.fillWidth: true
            }


            Label {
                id: targetDELabel
                color: "#ffffff"
                text: qsTr("DE (JNOW):")
                font.pointSize: 12
                font.bold: true
            }

            TextField {
                id: targetDEText
                width: 150
                height: 30
                Layout.maximumHeight: 30
                Layout.minimumHeight: 30
                Layout.fillWidth: true
            }


        }


        GridLayout {
            id: actionLayout
            width: 200
            height: 100
            Layout.maximumWidth: 200
            Layout.minimumWidth: 200
            Layout.fillHeight: false
            Layout.fillWidth: true
            rows: 2
            columns: 2

            Button {
                id: gotoButton
                text: qsTr("GOTO")
                Layout.fillWidth: true
            }

            Button {
                id: syncButton
                text: qsTr("SYNC")
                Layout.fillWidth: true
            }

            Button {
                id: parkButton
                text: qsTr("PARK")
                Layout.fillWidth: true
            }

            Button {
                id: unparkButton
                text: qsTr("UNPARK")
                Layout.fillWidth: true
            }
        }

        RowLayout {
            id: statusLayout
            width: 100
            height: 100
            Layout.fillWidth: true

            Label {
                id: statusLabel
                color: "#ffffff"
                text: qsTr("Status:")
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: statusText
                color: "#ffffff"
                text: qsTr("Idle")
                font.pointSize: 12
                font.bold: true
            }
        }

    }
}
