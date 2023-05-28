import QtQuick 2.5
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1
import QtQuick.Controls.Styles 1.4

Rectangle {
    id: rectangle
    objectName: "mountControlObject"

    color: "#000000"

    property color buttonColor: "silver"
    property color coordsColor: "gold"

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
        font.bold: false
        font.italic: false
        font.underline:false
        font.pointSize: 12
    }

    width:  (Qt.platform.os === "osx") ? fontMetrics.height * 13.5 /.75 : fontMetrics.height * 13.5
    height: (Qt.platform.os === "osx") ? fontMetrics.height * 29.5 /.75 : fontMetrics.height * 29.5

    MouseArea {
        anchors.fill: parent
        onClicked: mountMotionLayout.focus = true
         
        ColumnLayout {
            id: mainVerticalLayout
            anchors.fill: parent
            anchors.margins: fontMetrics.height * 0.25

            GridLayout {
                id: mountMotionLayout
                rowSpacing: fontMetrics.height * 0.05
                columnSpacing: fontMetrics.height * 0.05
                Layout.minimumHeight: fontMetrics.height * 8
                Layout.maximumHeight: fontMetrics.height * 8
                Layout.minimumWidth: fontMetrics.height * 10
                Layout.maximumWidth: fontMetrics.height * 10
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                columns: 3

                focus: true
                Keys.onPressed: {
                    event.accepted = true;
                    if (!event.isAutoRepeat)
                        onPressedChanged(event, true);
                }

                Keys.onReleased: {
                    event.accepted = true;
                    if (!event.isAutoRepeat)
                        onPressedChanged(event, false);
                }

                function setTimeout(delay_ms, cb) {
                    var component = Qt.createComponent("timer.qml");
                    var timer = component.createObject(rectangle, {x: 0, y: 0});

                    if (timer === null) {
                        console.error("Error creating timer object");
                    }

                    timer.callback = function() {
                        cb();
                        this.destroy();
                    };
                    timer.interval = delay_ms;
                    timer.running = true;
                }

                property var keyState: ({})
                 // -1 means not moving
                property var motionState: ({ns: -1, we: -1})
                // It appears [Qt.Key_Up] is not a valid subsituion in QML like Javascript in older versions?
                //property var filteredKeyState: ({[Qt.Key_Up]: false, [Qt.Key_Down]: false, [Qt.Key_Left]: false, [Qt.Key_Right]: false})
                // Replacing it with direct values and confirmed works with older Qt versions:
                // true means pressed
                property var filteredKeyState: {0x13: false, 0x15: false, 0x12: false, 0x14: false}
                function moveMount() {
                    var up = filteredKeyState[Qt.Key_Up];
                    var down = filteredKeyState[Qt.Key_Down];
                    var left = filteredKeyState[Qt.Key_Left];
                    var right = filteredKeyState[Qt.Key_Right];

                    var ns = -1;
                    var we = -1;
                    if (up !== down) {
                        if (up){
                            ns = 0;
                        } else if (down) {
                            ns = 1;
                        }
                        mount.motionCommand(0, ns, -1);
                        motionState.ns = ns;
                    } else if (motionState.ns !== -1) {
                        mount.motionCommand(1, motionState.ns, -1);
                        motionState.ns = -1;
                    }
                    if (left !== right) {
                        if (left){
                            we = 0;
                        } else if (right) {
                            we = 1;
                        }
                        mount.motionCommand(0, -1, we);
                        motionState.we = we;
                    } else if (motionState.we !== -1) {
                        mount.motionCommand(1, -1, motionState.we);
                        motionState.we = -1;
                    }

                    if (ns === 0) {
                        northRect.opacity = 1;
                    } else if (ns === 1) {
                        southRect.opacity = 1;
                    } else {
                        northRect.opacity = 0;
                        southRect.opacity = 0;
                    }
                    if (we === 0) {
                        westRect.opacity = 1;
                    } else if (we === 1) {
                        eastRect.opacity = 1;
                    } else {
                        westRect.opacity = 0;
                        eastRect.opacity = 0;
                    }
                }

                function deflicker(key, pressed) {
                    keyState[key] = pressed;
                    setTimeout(5, function() {
                        if (pressed === keyState[key] && pressed !== filteredKeyState[key]) {
                            filteredKeyState[key] = pressed;
                            moveMount();
                        }
                    });
                }

                function onPressedChanged(event, pressed) {
                    deflicker(event.key, pressed);
                }

                Button
                {
                    id: northWest
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

                    onClicked:
                    {
                        mountMotionLayout.focus = true;
                    }
                    
                    Image {
                        anchors.fill: parent
                        source: "go-northwest.png"
                    }
                }

                Button {
                    id: north
                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    Rectangle
                    {
                        anchors.fill: parent
                        color: north.pressed ? "red" : rectangle.buttonColor
                    }

                    Rectangle
                    {
                        id: northRect
                        opacity: 0
                        anchors.fill: parent
                        color: "red"
                    }

                    onPressedChanged:
                    {
                        north.pressed ? mount.motionCommand(0, 0, -1) : mount.motionCommand(1, 0, -1);
                    }

                    onClicked:
                    {
                        mountMotionLayout.focus = true;
                    }

                    Image {
                        anchors.fill: parent
                        source: "go-north.png"
                    }
                }

                Button {
                    id: northEast
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

                    onClicked:
                    {
                        mountMotionLayout.focus = true;
                    }

                    Image {
                        anchors.fill: parent
                        source: "go-northeast.png"
                    }
                }

            Button {
                    id: west
                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    Rectangle
                    {
                        anchors.fill: parent
                        color: west.pressed ? "red" : rectangle.buttonColor
                    }

                    Rectangle
                    {
                        id: westRect
                        opacity: 0
                        anchors.fill: parent
                        color: "red"
                    }

                    onPressedChanged:
                    {
                        west.pressed ? mount.motionCommand(0, -1, 0) : mount.motionCommand(1, -1, 0);
                    }
                    
                    onClicked:
                    {
                        mountMotionLayout.focus = true;
                    }

                    Image {
                        anchors.fill: parent
                        source: "go-west.png"
                    }
                }

                Button {
                    id: stop
                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    Rectangle
                    {
                        anchors.fill: parent
                        color: stop.pressed ? "red" : rectangle.buttonColor
                    }

                    Image {
                        anchors.fill: parent
                        source: "stop.png"
                    }

                    onClicked:
                    {
                        mount.abort();
                        mountMotionLayout.focus = true;
                    }


                }

                Button {
                    id: east
                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    Rectangle
                    {
                        anchors.fill: parent
                        color: east.pressed ? "red" : rectangle.buttonColor
                    }

                    Rectangle
                    {
                        id: eastRect
                        opacity: 0
                        anchors.fill: parent
                        color: "red"
                    }

                    onPressedChanged:
                    {
                        east.pressed ? mount.motionCommand(0, -1, 1) : mount.motionCommand(1, -1, 1);
                    }

                    onClicked:
                    {
                        mountMotionLayout.focus = true;
                    }

                    Image {
                        anchors.fill: parent
                        source: "go-east.png"
                    }
                }

                Button {
                    id: southWest
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

                    onClicked:
                    {
                        mountMotionLayout.focus = true;
                    }

                    Image {
                        anchors.fill: parent
                        source: "go-southwest.png"
                    }
                }

                Button {
                    id: south
                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    Rectangle
                    {
                        anchors.fill: parent
                        color: south.pressed ? "red" : rectangle.buttonColor
                    }

                    Rectangle
                    {
                        id: southRect
                        opacity: 0
                        anchors.fill: parent
                        color: "red"
                    }

                    onPressedChanged:
                    {
                        south.pressed ? mount.motionCommand(0, 1, -1) : mount.motionCommand(1, 1, -1);
                        mountMotionLayout.focus = true;
                    }

                    Image {
                        anchors.fill: parent
                        source: "go-south.png"
                    }
                }

                Button {
                    id: southEast
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

                    onClicked:
                    {
                        mountMotionLayout.focus = true;
                    }

                    Image {
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
                    renderType: Text.QtRendering
                }

                CheckBox
                {
                    id: updownReverse
                    style:CheckBoxStyle{
                        label:Text{
                            text: xi18n("Up/Down")
                            renderType: Text.QtRendering
                            color: "white"
                        }
                    }

                    objectName: "upDownCheckObject"
                    onClicked: mount.setUpDownReversed(checked)
                }

                CheckBox
                {
                    id: leftRightReverse
                    style:CheckBoxStyle{
                        label:Text{
                            text: xi18n("Left/Right")
                            renderType: Text.QtRendering
                            color: "white"
                        }
                    }
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
                    x: fontMetrics.height * 0.1
                    y: 0
                    width: fontMetrics.height * 1.5
                    objectName: "speedSliderObject"
                    Layout.fillWidth: true
                    Layout.maximumWidth: fontMetrics.height * 7.5
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
                    width: fontMetrics.height * 3.75
                    objectName: "speedLabelObject"
                    text: xi18n("1x")
                    renderType: Text.QtRendering
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                    Layout.maximumWidth: fontMetrics.height * 3.75
                    Layout.minimumWidth: fontMetrics.height * 3.75
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
                anchors.horizontalCenter: parent.Center
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                Layout.fillWidth: true
                columns: 4

                MouseArea {
                    anchors.fill: parent
                    property bool toggle: false
                    onClicked: {
                        if (toggle) {
                            targetRAText.text = ""
                            targetDEText.text = ""
                        }
                        else {
                            if (coordGroup.lastChecked == 0) {
                                targetRAText.text = raValue.text
                                targetDEText.text = deValue.text
                            }
                            if (coordGroup.lastChecked == 1) {
                                targetRAText.text = azValue.text
                                targetDEText.text = altValue.text
                            }
                            if (coordGroup.lastChecked == 2) {
                                targetRAText.text = haValue.text
                                targetDEText.text = deValue.text
                            }
                        }
                        toggle = !toggle
                    }
                }

                Label {
                    id: raLabel
                    text: xi18n("RA:")
                    renderType: Text.QtRendering
                    font.bold: true
                    color: "white"
                    fontSizeMode: Text.Fit
                }

                Label {
                    id: raValue
                    objectName: "raValueObject"
                    color: coordsColor
                    text: "00:00:00"
                    renderType: Text.QtRendering
                    Layout.fillWidth: true
                    font.pointSize: 11
                }

                Label {
                    id: azLabel
                    color: "#ffffff"
                    text: xi18n("AZ:")
                    renderType: Text.QtRendering
                    Layout.fillWidth: false
                    fontSizeMode: Text.Fit
                    font.bold: true
                }

                Label {
                    id: azValue
                    objectName: "azValueObject"
                    color: coordsColor
                    text: "00:00:00"
                    renderType: Text.QtRendering
                    Layout.fillWidth: true
                    font.pointSize: 11
                }

                Label {
                    id: deLabel
                    color: "#ffffff"
                    text: xi18n("DE:")
                    renderType: Text.QtRendering
                    fontSizeMode: Text.Fit
                    font.bold: true
                }

                Label {
                    id: deValue
                    objectName: "deValueObject"
                    color: coordsColor
                    text: "00:00:00"
                    renderType: Text.QtRendering
                    Layout.fillWidth: true
                    font.pointSize: 11
                }

                Label {
                    id: altLabel
                    color: "#ffffff"
                    text: xi18n("AL:")
                    renderType: Text.QtRendering
                    fontSizeMode: Text.Fit
                    font.bold: true
                }

                Label {
                    id: altValue
                    objectName: "altValueObject"
                    color: coordsColor
                    text: "00:00:00"
                    renderType: Text.QtRendering
                    Layout.fillWidth: true
                    font.pointSize: 11
                }

                Label {
                    id: haLabel
                    color: "#ffffff"
                    text: xi18n("HA:")
                    renderType: Text.QtRendering
                    fontSizeMode: Text.Fit
                    font.bold: true
                }

                Label {
                    id: haValue
                    objectName: "haValueObject"
                    color: coordsColor
                    text: "00:00:00"
                    renderType: Text.QtRendering
                    Layout.fillWidth: true
                    font.pointSize: 11
                }

                Label {
                    id: zaLabel
                    color: "#ffffff"
                    text: xi18n("ZA:")
                    renderType: Text.QtRendering
                    fontSizeMode: Text.Fit
                    font.bold: true
                }

                Label {
                    id: zaValue
                    objectName: "zaValueObject"
                    color: coordsColor
                    text: "00:00:00"
                    renderType: Text.QtRendering
                    Layout.fillWidth: true
                    font.pointSize: 11
                }
            }

            RowLayout
            {
                id: targetFindLayout
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter

                Label {
                    id: targetLabel
                    color: "#ffffff"
                    text: xi18n("Target:")
                    renderType: Text.QtRendering
                    verticalAlignment: Text.AlignVCenter
                    Layout.fillHeight: true
                    Layout.fillWidth: false
                    font.pointSize: 14
                    font.bold: true
                }

                TextField {
                    id: targetText
                    objectName: "targetTextObject"
                    placeholderText: "Click Find Icon"
                    style: TextFieldStyle {
                        textColor: "white"
                        placeholderTextColor: "gray"
                        renderType: Text.QtRendering
                        background: Rectangle {
                            color: "#202020"
                        }
                    }
                    readOnly: true
                    Rectangle
                    {
                        color: "black"
                        radius: fontMetrics.height * 0.25
                        anchors.fill: parent
                        clip: true
                        opacity: 0.5
                        border.color: "#D4AF37"
                        border.width: fontMetrics.height * 0.05
                    }
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillHeight: false
                    Layout.fillWidth: true
                    font.pointSize: 14
                    font.bold: true
                }

                Button {
                    id: findButton
                    objectName: "findButtonObject"
                    Layout.fillWidth: false
                    iconName: "view-history"
                    Layout.alignment: Qt.AlignRight
                    Layout.minimumHeight: fontMetrics.height * 1.4
                    Layout.maximumHeight: fontMetrics.height * 1.4
                    Layout.minimumWidth: fontMetrics.height * 1.5
                    Layout.maximumWidth: fontMetrics.height * 1.5
                    onClicked:
                    {
                        mount.findTarget()
                    }
                }

            }


            GridLayout {
                id: targetCoordsLayout
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                Layout.fillWidth: true
                columns: 2

                Label {
                    id: targetRALabel
                    objectName: "targetRALabelObject"
                    color: "#ffffff"
                    text: xi18n("RA:")
                    renderType: Text.QtRendering
                    font.pointSize: 14
                    font.bold: true
                }

                TextField {
                    id: targetRAText
                    objectName: "targetRATextObject"
                    placeholderText: "HH:MM:SS"
                    style: TextFieldStyle {
                        textColor: "white"
                        placeholderTextColor: "gray"
                        renderType: Text.QtRendering
                        background: Rectangle {
                            color: "#202020"
                        }
                    }
                    font.pointSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    Layout.minimumWidth: fontMetrics.height * 7
                    clip: true
                    font.bold: true
                    Layout.minimumHeight: fontMetrics.height * 1.4
                    Layout.fillWidth: true
                }

                Label {
                    id: targetDELabel
                    objectName: "targetDELabelObject"
                    color: "#ffffff"
                    text: xi18n("DE:")
                    renderType: Text.QtRendering
                    font.pointSize: 14
                    font.bold: true
                }

                TextField {
                    id: targetDEText
                    objectName: "targetDETextObject"
                    placeholderText: "DD:MM:SS"
                    style: TextFieldStyle {
                        textColor: "white"
                        placeholderTextColor: "gray"
                        renderType: Text.QtRendering
                        background: Rectangle {
                            color: "#202020"
                        }
                    }
                    font.pointSize: 14
                    width: fontMetrics.height * 7.5
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillHeight: false
                    Layout.minimumWidth: fontMetrics.height * 7
                    clip: true
                    font.bold: true
                    Layout.minimumHeight: fontMetrics.height * 1.4
                    Layout.fillWidth: true
                }

                Label {
                    id: coordLabel
                    text: xi18n("Type:")
                    renderType: Text.QtRendering
                }

                RowLayout
                {
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
                        style:RadioButtonStyle{
                            label:Text{
                                text: xi18n("RA/DE")
                                renderType: Text.QtRendering
                                color: "white"
                            }
                        }
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
                        style:RadioButtonStyle{
                            label:Text{
                                text: xi18n("AZ/AL")
                                renderType: Text.QtRendering
                                color: "white"
                            }
                        }
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
                        style:RadioButtonStyle{
                            label:Text{
                                text: xi18n("HA/DE")
                                renderType: Text.QtRendering
                                color: "white"
                            }
                        }
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
                    renderType: Text.QtRendering
                }

                RowLayout
                {
                    ExclusiveGroup { id: epochGroup }

                    RadioButton {
                        id: jnowCheck
                        objectName: "jnowCheckObject"
                        checked: true
                        style:RadioButtonStyle{
                            label:Text{
                                text: xi18n("JNow")
                                renderType: Text.QtRendering
                                color: "white"
                            }
                        }
                        exclusiveGroup: epochGroup
                    }

                    RadioButton {
                        id: j2000Check
                        objectName: "j2000CheckObject"
                        style:RadioButtonStyle{
                            label:Text{
                                text: xi18n("J2000")
                                renderType: Text.QtRendering
                                color: "white"
                            }
                        }
                        exclusiveGroup: epochGroup
                    }
                }
            }

            GridLayout {
                id: actionLayout
                Layout.fillHeight: false
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                columns: 2

                Button {
                    id: gotoButton
                    objectName: "gotoButtonObject"
                    text: xi18n("GOTO")
                    Layout.fillWidth: true

                    onClicked:
                    {
                        mount.slew(targetRAText.text, targetDEText.text);
                        mountMotionLayout.focus = true;
                    }
                }

                Button {
                    id: syncButton
                    objectName: "syncButtonObject"
                    text: xi18n("SYNC")
                    Layout.fillWidth: true

                    onClicked:
                    {
                        mount.sync(targetRAText.text, targetDEText.text);
                        mountMotionLayout.focus = true;
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
                    renderType: Text.QtRendering
                    font.pointSize: 12
                    font.bold: true
                }

                Label {
                    id: statusText
                    objectName: "statusTextObject"
                    color: "#ffffff"
                    text: xi18n("Idle")
                    renderType: Text.QtRendering
                    Layout.fillWidth: true
                    Layout.minimumWidth: fontMetrics.height * 5
                    font.pointSize: 12
                    font.bold: true
                }

                Button {
                    id: centerMount
                    Layout.minimumWidth: fontMetrics.height * 1.4
                    Layout.minimumHeight: fontMetrics.height * 1.4
                    iconName: "crosshairs"

                    onClicked:
                    {
                        mount.centerMount()
                    }
                }
            }
        }
    }
}

/*##^##
Designer {
    D{i:0;autoSize:true;height:480;width:640}
}
##^##*/
