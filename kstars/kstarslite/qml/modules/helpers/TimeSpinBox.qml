// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick.Controls 2.0
import QtQuick 2.6
import "../../constants" 1.0

SpinBox {
    id: control
    from: -41
    to: 41
    anchors.verticalCenter: parent.verticalCenter
    editable: false

    background: Rectangle {
        implicitWidth: 160
        color: Num.sysPalette.base
    }

    contentItem: Text {
        z: 2
        text: control.textFromValue(control.value, control.locale)
        anchors.verticalCenter: parent.verticalCenter

        font: control.font
        color: Num.sysPalette.text
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
    }

    property int spacingg: 0

    Connections {
        target: up
        onPressed: {
            console.log(up.pressed)
        }
    }

    up.indicator: Rectangle {
        z: 0
        x: control.mirrored ? -spacingg : parent.width - width + spacingg
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: 37
        implicitHeight: 37
        color: up.pressed || !enabled ? Num.sysPalette.light : Num.sysPalette.base
        border.color: up.pressed || !enabled ? Num.sysPalette.base : Num.sysPalette.light
        radius: 5

        Text {
            z: 1
            text: "+"
            font.pixelSize: control.font.pixelSize * 2
            color: up.pressed || !enabled ? Num.sysPalette.dark : Num.sysPalette.text
            anchors.fill: parent
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    down.indicator: Rectangle {
        z: 0
        x: control.mirrored ? parent.width - width + spacingg : -spacingg
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: 37
        implicitHeight: 37
        color: down.pressed || !enabled ? Num.sysPalette.light : Num.sysPalette.base
        border.color: down.pressed || !enabled ? Num.sysPalette.base : Num.sysPalette.light
        radius: 5

        Text {
            text: "-"
            font.pixelSize: control.font.pixelSize * 2
            color: down.pressed || !enabled ? Num.sysPalette.dark : Num.sysPalette.text
            anchors.fill: parent
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    property bool daysOnly: false

    onDaysOnlyChanged: {
        setDaysOnly()
    }

    Component.onCompleted: {
        setDaysOnly()
    }

    property double secs_per_day: 86400.0
    property double sidereal_year: 31558149.77
    property double tropical_year: 31556925.19

    property var timeString: []
    property var timeScale: []

    //TimeUnit control
    property int unitValue: 1
    property int allunits: 8
    property int dayunits: 5
    property int minUnitValue
    property int maxUnitValue
    property var unitStep: []

    function setDaysOnly() {
        var i = 0; //index for timeScale values

        timeScale[0] = 0.0
        if(!daysOnly) {
            timeScale[1] = 0.1;          // 0.1 sec
            timeScale[2] = 0.25;         // 0.25 sec
            timeScale[3] = 0.5;          // 0.5 sec
            timeScale[4] = 1.0;          // 1 sec (real-time)
            timeScale[5] = 2.0;          // 2 sec
            timeScale[6] = 5.0;          // 5 sec
            timeScale[7] = 10.0;         // 10 sec
            timeScale[8] = 20.0;         // 20 sec
            timeScale[9] = 30.0;         // 30 sec
            timeScale[10] = 60.0;        // 1 min
            timeScale[11] = 120.0;       // 2 min
            timeScale[12] = 300.0;       // 5 min
            timeScale[13] = 600.0;       // 10 min
            timeScale[14] = 900.0;       // 15 min
            timeScale[15] = 1800.0;      // 30 min
            timeScale[16] = 3600.0;      // 1 hr
            timeScale[17] = 7200.0;      // 2 hr
            timeScale[18] = 10800.0;     // 3 hr
            timeScale[19] = 21600.0;     // 6 hr
            timeScale[20] = 43200.0;     // 12 hr
            i = 20;
        }

        timeScale[i+1] = 86164.1;     // 23 hr 56 min
        timeScale[i+2] = secs_per_day;     // 1 day
        timeScale[i+3] = 2.*secs_per_day;  // 2 days
        timeScale[i+4] = 3.*secs_per_day;  // 3 days
        timeScale[i+5] = 5.*secs_per_day;  // 5 days
        timeScale[i+6] = 7.*secs_per_day;  // 1 week
        timeScale[i+7] = 14.*secs_per_day; //2 weeks
        timeScale[i+8] = 21.*secs_per_day; //3 weeks
        //Months aren't a simple measurement of time; I'll just use fractions of a year
        timeScale[i+9] = sidereal_year/12.0; // 1 month
        timeScale[i+10] = sidereal_year/6.0;  // 2 months
        timeScale[i+11] = 0.25*sidereal_year; // 3 months
        timeScale[i+12] = sidereal_year/3.0;  // 4 months
        timeScale[i+13] = 0.5*sidereal_year;  // 6 months
        timeScale[i+14] = 0.75*sidereal_year; // 9 months
        timeScale[i+15] = sidereal_year;       // 1 year
        timeScale[i+16] = 2.0*sidereal_year;   // 2 years
        timeScale[i+17] = 3.0*sidereal_year;   // 3 years
        timeScale[i+18] = 5.0*sidereal_year;   // 5 years
        timeScale[i+19] = 10.0*sidereal_year;  // 10 years
        timeScale[i+20] = 25.0*sidereal_year;  // 25 years
        timeScale[i+21] = 50.0*sidereal_year;  // 50 years
        timeScale[i+22] = 100.0*sidereal_year; // 100 years

        timeString = []
        if ( ! daysOnly ) {
            timeString.push( i18n( "0 secs" ));
            timeString.push( i18n( "0.1 secs" ));
            timeString.push( i18n( "0.25 secs" ));
            timeString.push( i18n( "0.5 secs" ));
            timeString.push( i18n( "1 sec" ));
            timeString.push( i18n( "2 secs" ));
            timeString.push( i18n( "5 secs" ));
            timeString.push( i18n( "10 secs" ));
            timeString.push( i18n( "20 secs" ));
            timeString.push( i18n( "30 secs" ));
            timeString.push( i18n( "1 min" ));
            timeString.push( i18n( "2 mins" ));
            timeString.push( i18n( "5 mins" ));
            timeString.push( i18n( "10 mins" ));
            timeString.push( i18n( "15 mins" ));
            timeString.push( i18n( "30 mins" ));
            timeString.push( i18n( "1 hour" ));
            timeString.push( i18n( "2 hrs" ));
            timeString.push( i18n( "3 hrs" ));
            timeString.push( i18n( "6 hrs" ));
            timeString.push( i18n( "12 hrs" ));
        } else {
            timeString.push( i18n( "0 days" ));
        }
        timeString.push( i18n( "1 sid day" ));
        timeString.push( i18n( "1 day" ));
        timeString.push( i18n( "2 days" ));
        timeString.push( i18n( "3 days" ));
        timeString.push( i18n( "5 days" ));
        timeString.push( i18n( "1 week" ));
        timeString.push( i18n( "2 wks" ));
        timeString.push( i18n( "3 wks" ));
        timeString.push( i18n( "1 month" ));
        timeString.push( i18n( "2 mths" ));
        timeString.push( i18n( "3 mths" ));
        timeString.push( i18n( "4 mths" ));
        timeString.push( i18n( "6 mths" ));
        timeString.push( i18n( "9 mths" ));
        timeString.push( i18n( "1 year" ));
        timeString.push( i18n( "2 yrs" ));
        timeString.push( i18n( "3 yrs" ));
        timeString.push( i18n( "5 yrs" ));
        timeString.push( i18n( "10 yrs" ));
        timeString.push( i18n( "25 yrs" ));
        timeString.push( i18n( "50 yrs" ));
        timeString.push( i18n( "100 yrs" ));

        if ( !daysOnly ) {
            from = -41
            to = 41
        } else {
            from = -21
            to = 21
        }

        if ( daysOnly ) {
            minUnitValue = 1-dayunits
            maxUnitValue = dayunits-1
            unitValue = 1 // Start out with days units

            unitStep[0] = 0;
            unitStep[1] = 1;
            unitStep[2] = 5;
            unitStep[3] = 8;
            unitStep[4] = 14;
        } else {
            minUnitValue = 1-allunits
            maxUnitValue = allunits-1
            unitValue = 1 // Start out with seconds units

            unitStep[0] = 0;
            unitStep[1] = 4;
            unitStep[2] = 10;
            unitStep[3] = 16;
            unitStep[4] = 21;
            unitStep[5] = 25;
            unitStep[6] = 28;
            unitStep[7] = 34;
        }

        value = 4
    }

    function increaseTimeUnit() {
        if(unitValue < maxUnitValue) {
            unitValue = unitValue + 1
            value = getUnitValue()
        }
    }

    function decreaseTimeUnit() {
        if(unitValue > minUnitValue) {
            unitValue = unitValue - 1
            value = getUnitValue()
        }
    }

    function getUnitValue() {
        var uval;
        if ( unitValue >= 0 ) uval = unitStep[ unitValue ];
        else uval = -1*unitStep[ Math.abs( unitValue ) ];
        return uval;
    }

    function unitValueFromNum( val ) {
        if ( val >= 0 ) return unitStep[ val ];
        else return -1*unitStep[ Math.abs( val ) ];
    }

    function getTimeScale() {
        return value > 0 ? timeScale[ value ] : -1.*timeScale[ Math.abs(value) ];
    }

    function reportChange() {
        console.log("Reporting new timestep value: " + getTimeScale())
        KStarsLite.scaleChanged( getTimeScale() )
    }

    textFromValue: function(value) {
        var result
        var posval = Math.abs( value )
        if ( posval > timeString.length - 1 ) posval = 4;

        result = timeString[ posval ];

    if ( value < 0 ) { result = '-' + result; }

        reportChange()
        return result;
    }

    valueFromText: function(text) {

        for (var i = 0; i < timeString.length; ++i) {
            if (text === timeString[i]) return i
            if (text.substring(1) === timeString[i]) return -1*i
        }

        return 0
    }
}
