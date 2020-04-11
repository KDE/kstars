import QtQuick 2.5
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1
import QtQuick.Controls.Styles 1.4

Rectangle {
    id: rectangle
    width: 240
    height: 570
    color: "#000000"

    property color buttonColor: "silver"
    property color coordsColor: "gold"

    ColumnLayout {
        id: mainVerticalLayout
        anchors.fill: parent
        anchors.margins: 5

        GridLayout {
            id: mountMotionLayout
            width: 200
            height: 150
            rowSpacing: 1
            columnSpacing: 1
            anchors.horizontalCenter: parent.horizontalCenter
            Layout.minimumHeight: 160
            Layout.minimumWidth: 210
            Layout.maximumHeight: 160
            Layout.maximumWidth: 210
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

                Rectangle
                {
                    anchors.fill: parent
                    color: northWest.pressed ? "red" : rectangle.buttonColor
                }

                onPressedChanged:
                {
                    northWest.pressed ? mount.motionCommand(0, 0, 0) : mount.motionCommand(1, 0, 0);
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

                Rectangle
                {
                    anchors.fill: parent
                    color: north.pressed ? "red" : rectangle.buttonColor
                }

                onPressedChanged:
                {
                    north.pressed ? mount.motionCommand(0, 0, -1) : mount.motionCommand(1, 0, -1);
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

                Rectangle
                {
                    anchors.fill: parent
                    color: northEast.pressed ? "red" : rectangle.buttonColor
                }

                onPressedChanged:
                {
                    northEast.pressed ? mount.motionCommand(0, 0, 1) : mount.motionCommand(1, 0, 1);
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

                Rectangle
                {
                    anchors.fill: parent
                    color: west.pressed ? "red" : rectangle.buttonColor
                }

                onPressedChanged:
                {
                    west.pressed ? mount.motionCommand(0, -1, 0) : mount.motionCommand(1, -1, 0);
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

                Rectangle
                {
                    anchors.fill: parent
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

                Rectangle
                {
                    anchors.fill: parent
                    color: east.pressed ? "red" : rectangle.buttonColor
                }

                onPressedChanged:
                {
                    east.pressed ? mount.motionCommand(0, -1, 1) : mount.motionCommand(1, -1, 1);
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

                Rectangle
                {
                    anchors.fill: parent
                    color: southWest.pressed ? "red" : rectangle.buttonColor
                }

                onPressedChanged:
                {
                    southWest.pressed ? mount.motionCommand(0, 1, 0) : mount.motionCommand(1, 1, 0);
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

                Rectangle
                {
                    anchors.fill: parent
                    color: south.pressed ? "red" : rectangle.buttonColor
                }

                onPressedChanged:
                {
                    south.pressed ? mount.motionCommand(0, 1, -1) : mount.motionCommand(1, 1, -1);
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

                Rectangle
                {
                    anchors.fill: parent
                    color: southEast.pressed ? "red" : rectangle.buttonColor
                }

                onPressedChanged:
                {
                    southEast.pressed ? mount.motionCommand(0, 1, 1) : mount.motionCommand(1, 1, 1);
                }

                Image {
                    width: 50
                    anchors.fill: parent
                    source: "go-southeast.png"
                }
            }
        }


        RowLayout {
            id: mountReverseLayout
            anchors.horizontalCenter: parent.horizontalCenter
            Layout.fillWidth: true

            Label
            {
                text: xi18n("Reverse")
            }

            CheckBox
            {
                id: updownReverse
                text: xi18n("Up/Down")
                objectName: "upDownCheckObject"
                onClicked: mount.setUpDownReversed(checked)
            }

            CheckBox
            {
                id: leftRightReverse
                text: xi18n("Left/Right")
                objectName: "leftRightCheckObject"
                onClicked: mount.setLeftRightReversed(checked)
            }

        }

        RowLayout {
            id: mountSpeedLayout
            anchors.horizontalCenter: parent.Center
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillHeight: false
            Layout.fillWidth: true

            Slider {
                id: speedSlider
                x: 5
                y: 0
                width: 30
                objectName: "speedSliderObject"
                Layout.fillWidth: true
                stepSize: 1
                minimumValue: 0
                maximumValue: 4
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
                text: xi18n("1x")
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                Layout.maximumWidth: 75
                Layout.minimumWidth: 75
                font.weight: Font.Bold
                font.bold: true
                font.pointSize: 12
                font.family: "Verdana"
                fontSizeMode: Text.Fit
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
                color: "white"
            }
        }


        GridLayout {
            id: mountCoordsLayout
            x: 0
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillHeight: false
            Layout.fillWidth: true
            rows: 2
            columns: 4

            Label {
                id: raLabel
                text: xi18n("RA:")
                font.pointSize: 9
                font.bold: true
                color: "white"
                fontSizeMode: Text.Fit
            }

            Label {
                id: raValue
                objectName: "raValueObject"
                color: coordsColor
                text: "00:00:00"
                Layout.fillWidth: true
                font.pointSize: 9
                fontSizeMode: Text.Fit
            }

            Label {
                id: azLabel
                color: "#ffffff"
                text: xi18n("AZ:")
                Layout.fillWidth: false
                fontSizeMode: Text.Fit
                font.bold: true
            }

            Label {
                id: azValue
                objectName: "azValueObject"
                color: coordsColor
                text: "00:00:00"
                Layout.fillWidth: true
                font.pointSize: 9
                fontSizeMode: Text.Fit
            }

            Label {
                id: deLabel
                color: "#ffffff"
                text: xi18n("DE:")
                font.pointSize: 10
                fontSizeMode: Text.Fit
                font.bold: true
            }

            Label {
                id: deValue
                objectName: "deValueObject"
                color: coordsColor
                text: "00:00:00"
                Layout.fillWidth: true
                font.pointSize: 9
                fontSizeMode: Text.Fit
            }

            Label {
                id: altLabel
                color: "#ffffff"
                text: xi18n("AL:")
                font.pointSize: 9
                fontSizeMode: Text.Fit
                font.bold: true
            }

            Label {
                id: altValue
                objectName: "altValueObject"
                color: coordsColor
                text: "00:00:00"
                Layout.fillWidth: true
                font.pointSize: 9
                fontSizeMode: Text.Fit
            }

            Label {
                id: haLabel
                color: "#ffffff"
                text: xi18n("HA:")
                font.pointSize: 9
                fontSizeMode: Text.Fit
                font.bold: true
            }

            Label {
                id: haValue
                objectName: "haValueObject"
                color: coordsColor
                text: "00:00:00"
                Layout.fillWidth: true
                font.pointSize: 9
                fontSizeMode: Text.Fit
            }

            Label {
                id: zaLabel
                color: "#ffffff"
                text: xi18n("ZA:")
                fontSizeMode: Text.Fit
                font.bold: true
            }

            Label {
                id: zaValue
                objectName: "zaValueObject"
                color: coordsColor
                text: "00:00:00"
                Layout.fillWidth: true
                font.pointSize: 9
                fontSizeMode: Text.Fit
            }
        }

        GridLayout {
            id: targetCoordsLayout
            height: 100
            Layout.fillWidth: true
            rows: 5
            columns: 3

            Label {
                id: targetLabel
                Layout.row: 1
                Layout.column: 1
                color: "#ffffff"
                text: xi18n("Target:")
                verticalAlignment: Text.AlignVCenter
                Layout.fillHeight: true
                Layout.fillWidth: false
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: targetText
                Layout.row: 1
                Layout.column: 2
                Layout.maximumHeight: 32
                Layout.minimumHeight: 32
                objectName: "targetTextObject"
                color: "#ffffff"
                Rectangle
                {
                    anchors.fill: parent
                    color: "black"
                    opacity: 0.5
                    border.color : "#D4AF37"
                    border.width : 1
                }

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                Layout.fillHeight: true
                Layout.fillWidth: true
                // Layout.minimumWidth: 75
                font.pointSize: 12
                font.bold: true
            }

            Button {
                id: findButton
                width: 32
                height: 32
                Layout.maximumHeight: 32
                Layout.minimumHeight: 32
                Layout.minimumWidth: 32
                Layout.maximumWidth: 32
                Layout.row: 1
                Layout.column: 3
                iconName: "view-history"

                onClicked:
                {
                    mount.findTarget()
                }
            }

            Label {
                id: targetRALabel
                color: "#ffffff"
                text: xi18n("RA/AZ:")
                font.pointSize: 12
                Layout.row: 2
                Layout.column: 1
            }

            TextField {
                id: targetRAText
                objectName: "targetRATextObject"
                placeholderText: "HH:MM:SS"
                width: 150
                Layout.minimumHeight: 30
                Layout.maximumHeight: 30
                Layout.fillWidth: true
                Layout.row: 2
                Layout.column: 2
            }


            Label {
                id: targetDELabel
                color: "#ffffff"
                text: xi18n("DE/AL:")
                font.pointSize: 12
                Layout.row: 3
                Layout.column: 1
            }

            TextField {
                id: targetDEText
                objectName: "targetDETextObject"
                placeholderText: "DD:MM:SS"
                width: 150
                height: 30
                Layout.maximumHeight: 30
                Layout.minimumHeight: 30
                Layout.fillWidth: true
                Layout.row: 3
                Layout.column: 2
            }

            Label {
                id: coordLabel
                text: xi18n("Type:")
                Layout.row: 4
                Layout.column: 1
            }

            RowLayout
            {
                Layout.row: 4
                Layout.column: 2
                ExclusiveGroup { id: coordGroup }
                RadioButton {
                    id: equatorialCheck
                    objectName: "equatorialCheckObject"
                    checked: true
                    text: xi18n("RA/DE")
                    exclusiveGroup: coordGroup

                    onCheckedChanged: checked ? targetRAText.placeholderText = "HH:MM:SS" : targetRAText.placeholderText = "DDD:MM:SS"
                }

                RadioButton {
                    id: horizontalCheck
                    objectName: "horizontalCheckObject"
                    text: xi18n("AZ/AL")
                    exclusiveGroup: coordGroup
                }
            }

            Label {
                id: epochLabel
                text: xi18n("Epoch:")
                Layout.row: 5
                Layout.column: 1
            }

            RowLayout
            {
                Layout.row: 5
                Layout.column: 2
                ExclusiveGroup { id: epochGroup }
                RadioButton {
                    id: jnowCheck
                    objectName: "jnowCheckObject"
                    checked: true
                    text: xi18n("JNow")
                    exclusiveGroup: epochGroup
                }

                RadioButton {
                    id: j2000Check
                    objectName: "j2000CheckObject"
                    text: xi18n("J2000")
                    exclusiveGroup: epochGroup
                }
            }
        }

        GridLayout {
            id: actionLayout
            width: 200
            height: 100
            anchors.horizontalCenter: parent.horizontalCenter
            Layout.maximumWidth: 250
            Layout.minimumWidth: 200
            Layout.fillHeight: false
            Layout.fillWidth: true
            rows: 2
            columns: 2

            Button {
                id: gotoButton
                text: xi18n("GOTO")
                Layout.fillWidth: true

                onClicked:
                {
                    mount.slew(targetRAText.text, targetDEText.text)
                }
            }

            Button {
                id: syncButton
                text: xi18n("SYNC")
                Layout.fillWidth: true

                onClicked:
                {
                    mount.sync(targetRAText.text, targetDEText.text)
                }
            }

            Button {
                id: parkButton
                objectName: "parkButtonObject"
                text: xi18n("PARK")
                Layout.fillWidth: true

                onClicked:
                {
                    mount.park()
                }
            }

            Button {
                id: unparkButton
                objectName: "unparkButtonObject"
                text: xi18n("UNPARK")
                Layout.fillWidth: true

                onClicked:
                {
                    mount.unpark()
                }
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
                text: xi18n("Status:")
                font.pointSize: 12
                font.bold: true
            }

            Label {
                id: statusText
                objectName: "statusTextObject"
                color: "#ffffff"
                text: xi18n("Idle")
                Layout.fillWidth: true
                Layout.minimumWidth: 100
                font.pointSize: 12
                font.bold: true
            }

            Button {
                id: centerMount
                Layout.maximumHeight: 32
                Layout.maximumWidth: 32
                Layout.minimumHeight: 32
                Layout.minimumWidth: 32
                iconName: "crosshairs"

                onClicked:
                {
                    mount.centerMount()
                }
            }
        }
    }
}
