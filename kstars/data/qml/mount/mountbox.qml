import QtQuick 2.5
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1
import QtQuick.Controls.Styles 1.4

Rectangle {
    id: rectangle
    objectName: "mountControlObject"
    color: "#000000"
    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

    property color buttonColor: "silver"
    property color coordsColor: "gold"

    ColumnLayout {
        id: mainVerticalLayout
        anchors.fill: parent
        anchors.margins: 5

        GridLayout {
            id: mountMotionLayout
            rowSpacing: 1
            columnSpacing: 1
            Layout.minimumHeight: 180
            Layout.minimumWidth: 230
            Layout.maximumHeight: 180
            Layout.maximumWidth: 230
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
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
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter

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
                Layout.maximumWidth: 150
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
            Layout.row: 1
            Layout.column:1

            Label {
                id: raLabel
                text: xi18n("RA:")
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
                font.pointSize: 11
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
                font.pointSize: 11
            }

            Label {
                id: deLabel
                color: "#ffffff"
                text: xi18n("DE:")
                fontSizeMode: Text.Fit
                font.bold: true
            }

            Label {
                id: deValue
                objectName: "deValueObject"
                color: coordsColor
                text: "00:00:00"
                Layout.fillWidth: true
                font.pointSize: 11
            }

            Label {
                id: altLabel
                color: "#ffffff"
                text: xi18n("AL:")
                fontSizeMode: Text.Fit
                font.bold: true
            }

            Label {
                id: altValue
                objectName: "altValueObject"
                color: coordsColor
                text: "00:00:00"
                Layout.fillWidth: true
                font.pointSize: 11
            }

            Label {
                id: haLabel
                color: "#ffffff"
                text: xi18n("HA:")
                fontSizeMode: Text.Fit
                font.bold: true
            }

            Label {
                id: haValue
                objectName: "haValueObject"
                color: coordsColor
                text: "00:00:00"
                Layout.fillWidth: true
                font.pointSize: 11
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
                font.pointSize: 11
            }
        }


        GridLayout {
            id: targetCoordsLayout
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
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
                font.pointSize: 14
                font.bold: true
            }

            TextField {
                id: targetText
                Layout.row: 1
                Layout.column: 2
                Layout.columnSpan: 2
                Layout.maximumHeight: 32
                Layout.minimumHeight: 32
                objectName: "targetTextObject"
                placeholderText: "Click Find Icon"
                readOnly: true
                Rectangle
                {
                    width: 200
                    color: "black"
                    radius: 6
                    anchors.fill: parent
                    clip: true
                    opacity: 0.5
                    border.color : "#D4AF37"
                    border.width : 1
                }
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                Layout.fillHeight: false
                Layout.fillWidth: true
                Layout.minimumWidth: 180
                font.pointSize: 14
                font.bold: true
            }

            Label {
                id: targetRALabel
                objectName: "targetRALabelObject"
                color: "#ffffff"
                text: xi18n("RA:")
                font.pointSize: 14
                font.bold: true
                Layout.row: 2
                Layout.column: 1
            }

            TextField {
                id: targetRAText
                objectName: "targetRATextObject"
                placeholderText: "HH:MM:SS"
                font.pointSize: 14
                horizontalAlignment: Text.AlignHCenter
                Layout.minimumWidth: 140
                Layout.maximumWidth: 140
                clip: true
                font.bold: true
                Layout.minimumHeight: 30
                Layout.maximumHeight: 30
                Layout.fillWidth: false
                Layout.row: 2
                Layout.column: 2
            }

            Button {
                id: findButton
                objectName: "findButtonObject"
                Layout.fillWidth: false
                Layout.row: 2
                Layout.column: 3
                iconName: "view-history"
                Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                Layout.minimumHeight: 30
                Layout.maximumHeight: 30
                Layout.minimumWidth: 30
                Layout.maximumWidth: 30
                onClicked:
                {
                    mount.findTarget()
                }
            }

            Label {
                id: targetDELabel
                objectName: "targetDELabelObject"
                color: "#ffffff"
                text: xi18n("DE:")
                font.pointSize: 14
                font.bold: true
                Layout.row: 3
                Layout.column: 1
            }

            TextField {
                id: targetDEText
                objectName: "targetDETextObject"
                placeholderText: "DD:MM:SS"
                font.pointSize: 14
                width: 150
                horizontalAlignment: Text.AlignHCenter
                Layout.maximumWidth: 140
                Layout.minimumWidth: 140
                clip: true
                font.bold: true
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
                Layout.columnSpan: 2

                ExclusiveGroup {
                    id: coordGroup
                    objectName: "coordGroupObject"
                    property int lastChecked: 0
                    property bool valid: false
                }

                RadioButton {
                    id: equatorialCheck
                    objectName: "equatorialCheckObject"
                    checked: true
                    text: xi18n("RA/DE")
                    exclusiveGroup: coordGroup
                    onCheckedChanged: {
                        if (checked) {
                            targetRALabel.text = xi18n("RA:")
                            targetDELabel.text = xi18n("DE:")
                            targetRAText.placeholderText = "HH:MM:SS"
                            if (targetRAText.text == "" ||
                                targetDEText.text == "") {
                                targetRAText.text = ""
                                targetDEText.text = ""
                            }
                            else {
                                if (coordGroup.lastChecked == 1)
                                    coordGroup.valid = mount.azAltToRaDec(
                                        targetRAText.text, targetDEText.text)
                                else
                                    coordGroup.valid = mount.haDecToRaDec(
                                        targetRAText.text)
                                if (!coordGroup.valid) {
                                    targetRAText.text = ""
                                    targetDEText.text = ""
                                }
                            }
                            targetRAText.focus = false
                            targetDEText.focus = false
                            coordGroup.lastChecked = 0
                        }
                    }
                }

                RadioButton {
                    id: horizontalCheck
                    exclusiveGroup: coordGroup
                    objectName: "horizontalCheckObject"
                    text: xi18n("AZ/AL")
                    checked: false
                    onCheckedChanged: {
                        if (checked) {
                            targetText.text = ""
                            targetRALabel.text = xi18n("AZ:")
                            targetDELabel.text = xi18n("AL:")
                            targetRAText.placeholderText = "DDD:MM:SS"
                            if (targetRAText.text == "" ||
                                targetDEText.text == "") {
                                targetRAText.text = ""
                                targetDEText.text = ""
                            }
                            else {
                                if (coordGroup.lastChecked == 0)
                                    coordGroup.valid = mount.raDecToAzAlt(
                                        targetRAText.text, targetDEText.text)
                                else
                                    coordGroup.valid = mount.haDecToAzAlt(
                                        targetRAText.text, targetDEText.text)
                                if (!coordGroup.valid) {
                                    targetRAText.text = ""
                                    targetDEText.text = ""
                                }
                            }
                            targetRAText.focus = false
                            targetDEText.focus = false
                            coordGroup.lastChecked = 1
                        }
                    }
                }

                RadioButton {
                    id: haEquatorialCheck
                    text: xi18n("HA/DE")
                    exclusiveGroup: coordGroup
                    objectName: "haEquatorialCheckObject"
                    checked: false
                    onCheckedChanged: {
                        if (checked) {
                            targetText.text = ""
                            targetRALabel.text = xi18n("HA:")
                            targetDELabel.text = xi18n("DE:")
                            targetRAText.placeholderText = "HH:MM:SS"
                            if (targetRAText.text == "" ||
                                targetDEText.text == "") {
                                targetRAText.text = ""
                                targetDEText.text = ""
                            }
                            else {
                                if (coordGroup.lastChecked == 1)
                                    coordGroup.valid = mount.azAltToHaDec(
                                        targetRAText.text, targetDEText.text)
                                else
                                    coordGroup.valid = mount.raDecToHaDec(
                                        targetRAText.text)
                                if (!coordGroup.valid) {
                                    targetRAText.text = ""
                                    targetDEText.text = ""
                                }
                            }
                            targetRAText.focus = false
                            targetDEText.focus = false
                            coordGroup.lastChecked = 2
                        }
                    }
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
            Layout.fillHeight: false
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            rows: 2
            columns: 2

            Button {
                id: gotoButton
                objectName: "gotoButtonObject"
                text: xi18n("GOTO")
                Layout.fillWidth: true

                onClicked:
                {
                    mount.slew(targetRAText.text, targetDEText.text)
                }
            }

            Button {
                id: syncButton
                objectName: "syncButtonObject"
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
            Layout.fillHeight: false
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter

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
