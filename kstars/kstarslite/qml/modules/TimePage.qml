// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick.Controls 2.0
import QtQuick 2.6
import QtQuick.Layouts 1.1
import Qt.labs.calendar 1.0
import "../constants" 1.0

KSPage {
    //Change it to Popup when it will become more stable
    id: timePopup
    title: xi18n("Set Time")

    property date currentDate: new Date()

    property int userYear: tumblerYear.model[tumblerYear.currentIndex]
    property int userMonth: tumblerMonth.currentIndex
    property int userDay: tumblerDay.currentIndex + 1
    property int userHour: tumblerHour.currentIndex
    property int userMinutes: tumblerMinute.currentIndex
    //property int userWeek:
    signal updateCalendar()
    signal updateTumblers()

    function range(start, end) {
        var list = [];
        for (var i = start; i <= end; i++) {
            list.push(i);
        }
        return list
    }

    function getWeekNumber(year,month,day) {
        var d = new Date(year,month,day);
        d.setHours(0,0,0);
        // Set to nearest Thursday: current date + 4 - current day number
        // Make Sunday's day number 7
        d.setDate(d.getDate() + 4 - (d.getDay()||7));
        // Get first day of year
        var yearStart = new Date(d.getFullYear(),0,1);
        // Calculate full weeks to nearest Thursday
        var weekNo = Math.ceil(( ( (d - yearStart) / 86400000) + 1)/7)
        // Return array of year and week number
        return weekNo;
    }

    function weeksInYear(year) {
        var d = new Date(year, 11, 31);
        var week = getWeekNumber(d)[1];
        return week == 1? getWeekNumber(d.setDate(24))[1] : week;
    }

    function daysInMonth(month, year) {
        return new Date(year, month, 0).getDate();
    }

    function setCurrentDate() {
        currentDate = new Date()
        tumblerYear.setYear(currentDate.getFullYear())
        tumblerMonth.setMonth(currentDate.getMonth())
        tumblerDay.setDay(currentDate.getDate())
        tumblerHour.setHour(currentDate.getHours())
        tumblerMinute.setMinute(currentDate.getMinutes())
        tumblerWeek.setWeek(currentDate.getFullYear(), currentDate.getMonth(), currentDate.getDate())
    }

    GridLayout {
        anchors.centerIn: parent
        id: timeGrid
        columnSpacing: 10

        flow: !window.isPortrait ? GridLayout.LeftToRight : GridLayout.TopToBottom

        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 0
            RowLayout {
                anchors {
                    left: parent.left
                    right: parent.right
                }

                Button {
                    text: "<"
                    anchors{
                        left: parent.left
                        verticalCenter: parent.verticalCenter
                    }
                    onClicked: {
                        if(userMonth - 1 == -1) {
                            tumblerMonth.setMonth(11)
                            tumblerYear.setYear(userYear - 1)
                        } else {
                            tumblerMonth.setMonth(userMonth - 1)
                        }
                    }
                }

                KSText {
                    font.pointSize: 14
                    text: Qt.locale().standaloneMonthName(userMonth) + " " + monthGrid.year
                    anchors.centerIn: parent
                }

                Button {
                    text: ">"
                    anchors{
                        verticalCenter: parent.verticalCenter
                        right: parent.right
                    }
                    onClicked: {
                        if(userMonth + 1 == 12) {
                            tumblerMonth.setMonth(0)
                            tumblerYear.setYear(userYear + 1)
                        } else {
                            tumblerMonth.setMonth(userMonth + 1)
                        }
                    }
                }
            }

            GridLayout {
                id: calendar
                columns: 2
                /*Layout.fillHeight: true
                Layout.fillWidth: true*/
                Layout.minimumHeight: 150
                Layout.minimumWidth: 150
                /*Layout.maximumHeight: 250
                Layout.maximumWidth: 250*/

                DayOfWeekRow {
                    Layout.column: 1
                    Layout.fillWidth: true
                }

                WeekNumberColumn {
                    id: weeks
                    month: monthGrid.month
                    year: monthGrid.year
                    locale: monthGrid.locale
                    Layout.fillHeight: true
                }

                MonthGrid {
                    id: monthGrid
                    spacing: 0
                    month: userMonth
                    year: userYear
                    property int day: userDay
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    delegate: Rectangle {
                        id: rect
                        height: 30
                        width: 30
                        color: "#00FFFFFF"
                        property color highlightColor: Num.sysPalette.highlight
                        property int selectedDay // Initialize with userDay

                        Connections {
                            target: timePopup
                            onUserDayChanged: {
                                if(timePopup.userDay == model.day && model.month === monthGrid.month)
                                    rect.color = highlightColor
                                else rect.color = "#00FFFFFF"
                            }
                        }

                        border {
                            width: 1
                            color: "#DCDCDC"
                        }

                        KSText {
                            anchors.centerIn: parent
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            opacity: model.month === monthGrid.month ? 1 : 0.5
                            text: model.day
                            font: monthGrid.font
                        }

                        MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                if(model.month != monthGrid.month) {
                                    monthGrid.month = model.month
                                    userMonth = model.month
                                } else {
                                    rect.color = highlightColor
                                    userDay = model.day
                                }
                                updateTumblers()
                            }
                        }
                    }

                }
            }
        }

        ColumnLayout {
            id: tumblersCol

            GridLayout {
                id: tumblersGrid
                //FIX IT: Didn't find a better way to make tumblers smaller
                property int tumblerWidth: 35
                property int tumblerHeight: 50
                Layout.fillWidth: true
                flow: window.isPortrait ? Flow.LeftToRight : Flow.TopToBottom

                GroupBox {
                    id: dateGB
                    Layout.fillWidth: true

                    RowLayout {
                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                        Layout.fillWidth: true

                        ColumnLayout {
                            KSLabel {
                                id:labelYear
                                text: xi18n("Year")
                            }
                            Tumbler {
                                id: tumblerYear
                                implicitHeight: tumblersGrid.tumblerHeight
                                implicitWidth: tumblersGrid.tumblerWidth

                                model: range(1616,2500)
                                anchors.horizontalCenter: labelYear.horizontalCenter
                                property bool setByCalendar: false
                                Connections {
                                    target: timePopup
                                    onUpdateTumblers: {
                                        tumblerYear.setByCalendar = true
                                        tumblerYear.setYear(userYear)
                                    }
                                }

                                Component.onCompleted: {
                                    setYear(currentDate.getFullYear())
                                }

                                onCurrentIndexChanged: {
                                    if(!setByCalendar) {
                                        //userDate.setFullYear(model[currentIndex])
                                        updateCalendar()
                                    }
                                    setByCalendar = false
                                    tumblerMonth.checkDays()
                                }

                                function setYear(year) {
                                    var index = model.indexOf(year)
                                    if(index != -1) currentIndex = index
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillHeight: true
                            KSLabel {
                                id:labelMonth
                                text: xi18n("Month")
                            }

                            Tumbler {
                                id: tumblerMonth
                                model: range(1,12)
                                implicitHeight: tumblersGrid.tumblerHeight
                                implicitWidth: tumblersGrid.tumblerWidth

                                anchors.horizontalCenter: labelMonth.horizontalCenter
                                property bool setByCalendar: false
                                //implicitWidth: labelMonth.width

                                Component.onCompleted: {
                                    setMonth(currentDate.getMonth())
                                }

                                Connections {
                                    target: timePopup
                                    onUpdateTumblers: {
                                        tumblerMonth.setByCalendar = true
                                        tumblerMonth.setMonth(userMonth)//userDate.getMonth())
                                    }
                                }

                                onCurrentIndexChanged: {
                                    userMonth = currentIndex
                                    checkDays()
                                }

                                //Called whenever we change month or year. Handles number of days in month
                                function rangeDays() {
                                    var month = currentIndex+1
                                    var year = tumblerYear.model[tumblerYear.currentIndex]

                                    return range(1,daysInMonth(month,year))
                                }

                                function setMonth(month) {
                                    if(month >= 0 && month <= 11)
                                        currentIndex = month
                                }

                                function checkDays() {
                                    tumblerDay.changeModel(rangeDays())
                                }
                            }
                        }

                        ColumnLayout {
                            visible: false
                            KSLabel {
                                id:labelWeek
                                text: xi18n("Week")
                            }

                            Tumbler {
                                id: tumblerWeek
                                model: range(1,52)
                                anchors.horizontalCenter: labelWeek.horizontalCenter
                                implicitHeight: tumblersGrid.tumblerHeight
                                implicitWidth: tumblersGrid.tumblerWidth
                                property bool setByCalendar: false

                                onCurrentIndexChanged: {
                                    if(!setByCalendar) {
                                        //Start counting weeks from the beginning of year
                                        /*var day = userDay
                                        day.setMonth(0)
                                        day.setDate(1)

                                        day.setDate((currentIndex+1)*7)
                                        tumblerMonth.setMonth(day.getMonth())
                                        tumblerDay.setDay(day.getDate())

                                        userDate = day

                                        updateCalendar()*/
                                    }
                                    setByCalendar = false
                                }

                                Connections {
                                    target: timePopup
                                    onUpdateTumblers: {
                                        tumblerWeek.setByCalendar = true
                                        tumblerWeek.setWeek(userYear, userMonth + 1, userDay)
                                    }
                                }

                                function setWeek(year, month, day) {
                                    currentIndex = getWeekNumber(year, month, day) - 1
                                }
                            }
                        }

                        ColumnLayout {
                            KSLabel {
                                id:labelDay
                                text: xi18n("Day")
                            }

                            Tumbler {
                                id: tumblerDay
                                implicitHeight: tumblersGrid.tumblerHeight
                                implicitWidth: tumblersGrid.tumblerWidth
                                anchors.horizontalCenter: labelDay.horizontalCenter

                                model: range(1,daysInMonth(currentDate.getMonth() + 1,currentDate.getFullYear()))
                                property bool setByCalendar: false

                                function changeModel(newModel) {
                                    var prevIndex = currentIndex
                                    if(!model || model.length !== newModel.length) {
                                        model = newModel
                                        if(prevIndex >= 0 && prevIndex < model.length) currentIndex = prevIndex
                                    }
                                }

                                Connections {
                                    target: timePopup
                                    onUpdateTumblers: {
                                        tumblerDay.setByCalendar = true
                                        tumblerDay.setDay(userDay)
                                    }
                                }

                                onCurrentIndexChanged: {
                                    userDay = currentIndex + 1
                                }

                                Component.onCompleted: {
                                    setDay(currentDate.getDate())
                                }

                                function setDay(day) {
                                    if(day > 0 && day <= model.length)
                                        currentIndex = day - 1
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    anchors {
                        left: parent.left
                        right: parent.right
                    }

                    Button {
                        visible: !window.isPortrait
                        anchors {
                            left: parent.left
                            bottom: parent.bottom
                        }

                        onClicked: {
                            setCurrentDate()
                        }

                        text: xi18n("Now")
                    }

                    GroupBox {
                        id: timeGB
                        anchors.right: parent.right

                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                KSLabel {
                                    id: labelHour
                                    text: xi18n("Hour")
                                }

                                Tumbler {
                                    id: tumblerHour
                                    model: 24
                                    implicitHeight: tumblersGrid.tumblerHeight
                                    implicitWidth: tumblersGrid.tumblerWidth
                                    anchors.horizontalCenter: labelHour.horizontalCenter

                                    delegate: KSText {
                                        text: modelData < 10 ? "0" + modelData : modelData
                                        font: tumblerHour.font
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        opacity: 1.0 - Math.abs(Tumbler.displacement) / (tumblerHour.visibleItemCount / 2)
                                    }

                                    Component.onCompleted: {
                                        setHour(currentDate.getHours())
                                    }

                                    function setHour(hour) {
                                        currentIndex = hour
                                    }
                                }
                            }

                            ColumnLayout {
                                KSLabel {
                                    id:labelMinute
                                    text: xi18n("Min.")
                                }

                                Tumbler {
                                    id: tumblerMinute
                                    model: 60
                                    implicitHeight: tumblersGrid.tumblerHeight
                                    implicitWidth: tumblersGrid.tumblerWidth
                                    anchors.horizontalCenter: labelMinute.horizontalCenter

                                    delegate: KSText {
                                        text: modelData < 10 ? "0" + modelData : modelData
                                        font: tumblerHour.font
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        opacity: 1.0 - Math.abs(Tumbler.displacement) / (tumblerHour.visibleItemCount / 2)
                                    }

                                    Component.onCompleted: {
                                        setMinute(currentDate.getMinutes())
                                    }

                                    function setMinute(minutes) {
                                        currentIndex = minutes
                                    }
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                //height: childrenRect.height
                Layout.fillWidth: true
                anchors {
                    left: parent.left
                    right: parent.right
                }

                Button {
                    visible: window.isPortrait
                    anchors {
                        left: parent.left
                    }

                    onClicked: {
                        setCurrentDate()
                        console.log(Projector)
                    }

                    text: xi18n("Now")
                }

                Button {
                    visible: !window.isPortrait
                    text: xi18n("Ok")
                    anchors {
                        left: parent.left
                    }
                    onClicked: {
                        var date = new Date(userYear, userMonth, userDay, userHour, userMinutes)
                        KStarsLite.slotSetTime(date)
                        skyMapLite.notification.showNotification("Setting time to " + date)
                        stackView.pop()
                    }
                }

                Row {
                    anchors.right: parent.right
                    spacing: 5

                    Button {
                        visible: window.isPortrait
                        text: xi18n("Ok")
                        onClicked: {
                            var date = new Date(userYear, userMonth, userDay, userHour, userMinutes)
                            KStarsLite.slotSetTime(date)
                            skyMapLite.notification.showNotification("Setting time to " + date)
                            stackView.pop()
                        }
                    }

                    Button {
                        text: xi18n("Cancel")
                        onClicked: {
                            stackView.pop()
                        }
                    }
                }
            }

        }
    }
}
